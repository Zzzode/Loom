// Official-owned ER107 public Markdown observations. Frozen source/geometry
// authority supplies expectations; producer code and Native evidence are not
// inputs. No matrix admission or global verification precedes owned captures.
#pragma once
#include "mermaid_er107_authority.hpp"

namespace loom::test::mermaid_er107::official {
namespace auth = authority;
namespace source = loom::test::mermaid_er107;
namespace json = loom::utils::json;
namespace theme = loom::ui::design::theme;
namespace fs = std::filesystem;
inline constexpr std::string_view kContract = "ER107/complete-original/v1";
inline constexpr std::string_view kSentinel = "AFTER_OFFICIAL_ER107";
inline constexpr std::string_view kEvidence = "mermaid-official-er107.jsonl";
inline constexpr std::string_view kVerdict = "mermaid-er107-official-verdict.json";
inline constexpr std::string_view kSourceHelperSha =
    "1593280ffaf27f9b9115a898557734693f58b181b81c64067ff9c399ed3cae46";
using Frames = std::array<auth::Frame, 5>;

inline void need(bool good, std::string_view message) {
    auth::require(good, message);
}

inline std::string hash(std::string_view raw) {
    return loom::utils::crypto::sha256(raw);
}

inline std::string read(const fs::path& path, std::size_t cap = kMaxFactBytes) {
    need(!fs::is_symlink(fs::symlink_status(path)) && fs::is_regular_file(path),
         "source: Official ER107 regular input");
    need(fs::file_size(path) <= cap, "resource: Official ER107 input byte bound");
    std::ifstream stream(path, std::ios::binary);
    need(stream.good(), "source: Official ER107 input open");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

struct Group {
    Frames frames;
    std::size_t captures = 0;
};

struct Captures {
    std::array<Group, 3> groups;
    bool cache_hit = false;

    std::size_t count() const {
        std::size_t n = 0;
        for (const auto& group : groups)
            n += group.captures;
        return n;
    }

    bool complete() const {
        return count() == 15 &&
               std::ranges::all_of(groups, [](const Group& group) { return group.captures == 5; });
    }
};

struct Documents {
    json::JsonDoc source, terminal, presentation, manifest, prior;
    std::string contract;

    Documents() {
        const fs::path fixtures = fs::path(LOOM_TESTS_DIR) / "fixtures";
        const auto s = read(fixtures / std::string(kFactsPath)),
                   t = read(fixtures / std::string(auth::kProjectionPath));
        const auto p = read(fixtures / std::string(auth::kPresentationPath)),
                   m = read(fixtures / "mermaid_official/manifest.json"),
                   old = read(fixtures / std::string(kPriorPath));
        OracleWork bytes;
        auth::verify_asset_bytes(s, t, p, m, old, bytes);
        contract = read(fs::path(LOOM_TESTS_DIR).parent_path() /
                            "docs/design/mermaid-er107-terminal-contract.md",
                        262144);
        need(hash(contract) == auth::kContractSnapshotSha,
             "source: Official ER107 frozen B2 contract hash");
        need(hash(read(fs::path(LOOM_TESTS_DIR) / "support/mermaid_er107_source.hpp", 131072)) ==
                 kSourceHelperSha,
             "source: Official ER107 B1 source helper pin");
        // Parsing immutable bytes does not bind typed cases or validate root
        // components. Those stages are explicitly deferred below.
        auto sd = json::parse(s), td = json::parse(t), pd = json::parse(p), md = json::parse(m),
             od = json::parse(old);
        need(sd && td && pd && md && od, "source: Official ER107 immutable document JSON");
        source = std::move(*sd);
        terminal = std::move(*td);
        presentation = std::move(*pd);
        manifest = std::move(*md);
        prior = std::move(*od);
    }

    auth::BoundCase bind(std::size_t index, std::string_view raw, const Captures& captured,
                         OracleWork& work) const {
        need(index < kPins.size() && captured.complete(),
             "protocol: ER107 all fifteen owned captures precede case binding");
        const auto& pin = kPins[index];
        need(auth::kPins[index].path == pin.path,
             "source: ER107 source/projection selection identity");
        return auth::bind_case(source.root(), terminal.root(), manifest.root(), prior.root(),
                               pin.path, raw, work);
    }

    void finish(const std::array<bool, 107>& captures_complete) const {
        need(std::ranges::all_of(captures_complete, [](bool done) { return done; }),
             "protocol: ER107 all107 capture groups precede aggregate binding");
        OracleWork source_root, projection_root;
        auth::validate_roots(source.root(), terminal.root(), presentation.root(), manifest.root(),
                             prior.root(), source_root, projection_root);
        OracleWork excerpts;
        auth::validate_contract_excerpts(terminal.root(), contract, excerpts);
    }
};

inline std::optional<std::size_t> select(std::size_t global, std::string_view path,
                                         std::string_view raw_sha) {
    for (std::size_t i = 0; i < kPins.size(); ++i)
        if (kPins[i].path == path) {
            need(kPins[i].global == static_cast<int>(global) && kPins[i].raw_sha == raw_sha,
                 "source: ER107 compiled original/global/raw selector");
            return i;
        }
    return std::nullopt;
}

struct ThemeGuard {
    theme::Theme original = theme::current_theme();

    ~ThemeGuard() { theme::set_theme(original); }
};

inline auth::Surfaces surfaces() {
    const auto snapshot = theme::current_theme();
    const auto& p = *snapshot.palette;
    const std::array<ftxui::Color, 7> fg{p.text,    p.text, p.danger,       p.warning,
                                         p.success, p.info, p.accent_purple};
    const std::array<ftxui::Color, 7> bg{ftxui::Color::Default, p.surface_tint_2,  p.danger_tint_bg,
                                         p.warning_tint_bg,     p.success_tint_bg, p.info_tint_bg,
                                         p.surface_tint_4};
    auth::Surfaces out;
    for (std::size_t i = 0; i < 7; ++i) {
        out.foreground[i] = fg[i].Print(false);
        out.background[i] = bg[i].Print(true);
    }
    out.inherited_background = bg[0].Print(true);
    out.inherited_dim = 0;
    return out;
}

inline auth::Frame capture(ftxui::Element node, int width, int height, OracleWork& work) {
    need(width > 0 && width <= 240 && height > 0 && height <= 1040,
         "resource: ER107 public capture extent");
    const auto area = static_cast<std::size_t>(width * height);
    source::detail::pay(work, area, sizeof(auth::ObservedCell) + 64, 1024);
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, node);
    auth::Frame out;
    out.width = width;
    out.height = height;
    out.cells.reserve(area);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const auto& p = screen.PixelAt(x, y);
            const auto other = static_cast<std::uint16_t>(
                (p.blink ? 1 : 0) | (p.inverted ? 4 : 0) | (p.underlined ? 8 : 0) |
                (p.underlined_double ? 16 : 0) | (p.strikethrough ? 32 : 0));
            auto fg = p.foreground_color.Print(false), bg = p.background_color.Print(true);
            need(p.character.size() <= 4096 && fg.size() <= 128 && bg.size() <= 128,
                 "resource: ER107 captured glyph/color bytes");
            source::detail::pay(work, p.character.size() + fg.size() + bg.size(), 1);
            out.cells.push_back({p.character, std::move(fg), std::move(bg), p.bold,
                                 static_cast<std::uint16_t>(p.dim ? 2 : 0), other, p.hyperlink});
        }
    return out;
}

inline std::string fence(std::string_view raw, std::string_view language) {
    std::size_t run = 0, longest = 0;
    for (const auto c : raw) {
        run = c == '`' ? run + 1 : 0;
        longest = std::max(longest, run);
    }
    const std::string ticks(std::max(std::size_t{3}, longest + 1), '`');
    return ticks + std::string(language) + "\n" + std::string(raw) + "\n" + ticks + "\n";
}

inline void observe(Group& out, std::string_view raw, OracleWork& work,
                    loom::ui::mermaid::LayoutRowsCache* cache = nullptr, bool dim = true) {
    need(out.captures == 0, "protocol: ER107 each group owns fresh observations");
    loom::ui::MarkdownOptions options;
    options.dim_color = dim;
    options.mermaid_rows_cache = cache;
    auto drawing = loom::ui::render_markdown(fence(raw, "mermaid"), options);
    options.mermaid_rows_cache = nullptr;
    auto ordinary = loom::ui::render_markdown(fence(raw, "mermaid-source"), options);
    const auto snapshot = theme::current_theme();
    auto footer = [&](ftxui::Element body) {
        return ftxui::vbox({std::move(body), ftxui::text(std::string(kSentinel))}) |
               ftxui::color(snapshot.palette->text) | ftxui::bgcolor(ftxui::Color::Default);
    };
    auto node = footer(drawing), plain = footer(ordinary);
    node->ComputeRequirement();
    plain->ComputeRequirement();
    const int height =
        std::max(40, std::max(node->requirement().min_y, plain->requirement().min_y) + 8);
    need(height <= 1040, "resource: ER107 public requirements exceed capture bound");
    constexpr std::array<int, 5> widths{240, 40, 240, 240, 40};
    for (std::size_t i = 0; i < 5; ++i) {
        out.frames[i] = capture(i < 3 ? node : plain, widths[i], height, work);
        ++out.captures;
    }
}

inline void observe(Captures& out, std::string_view raw, OracleWork& work) {
    need(raw.find(kSentinel) == std::string_view::npos,
         "source: ER107 complete raw source/sentinel collision");
    loom::ui::mermaid::LayoutRowsCache cache(4, 4 * 1024 * 1024);
    observe(out.groups[0], raw, work);
    observe(out.groups[1], raw, work, &cache);
    observe(out.groups[2], raw, work, &cache);
    out.cache_hit = cache.hits() > 0;
    need(out.cache_hit, "protocol: ER107 actual public Markdown cache hit");
}

inline std::string cell_bytes(const auth::Frame& frame) {
    std::string raw;
    for (const auto& cell : frame.cells) {
        for (const auto* text : {&cell.glyph, &cell.foreground, &cell.background}) {
            raw += std::to_string(text->size());
            raw += ':';
            raw += *text;
        }
        raw += cell.bold ? '1' : '0';
        for (const auto n :
             {static_cast<unsigned>(cell.dim), static_cast<unsigned>(cell.other_attributes),
              static_cast<unsigned>(cell.hyperlink)}) {
            raw += ':';
            raw += std::to_string(n);
        }
        raw += ';';
    }
    return raw;
}

inline void publish(const Captures& captured, json::JsonMutVal record, OracleWork& work) {
    auto groups = record.make_obj();
    constexpr std::array<std::string_view, 3> group_names{"uncached", "cache_miss", "cache_hit"};
    constexpr std::array<std::string_view, 5> states{"wide_240", "narrow_40", "restored_240",
                                                     "ordinary_240", "ordinary_40"};
    for (std::size_t g = 0; g < captured.groups.size(); ++g) {
        const auto& group = captured.groups[g];
        auto value = record.make_obj(), frames = record.make_arr();
        value.set("captures", static_cast<std::int64_t>(group.captures));
        for (std::size_t n = 0; n < group.captures; ++n) {
            const auto& frame = group.frames[n];
            source::detail::pay(work, frame.cells.size(), 192, 1024);
            auto entry = record.make_obj();
            entry.set("state", states[n]);
            entry.set("width", frame.width);
            entry.set("height", frame.height);
            entry.set("cell_sha256", hash(cell_bytes(frame)));
            auto runs = record.make_arr();
            for (std::size_t begin = 0; begin < frame.cells.size();) {
                std::size_t end = begin + 1;
                while (end < frame.cells.size() && frame.cells[end] == frame.cells[begin])
                    ++end;
                const auto& c = frame.cells[begin];
                auto run = record.make_arr();
                run.append(record.make_num(static_cast<std::int64_t>(end - begin)));
                for (const auto* text : {&c.glyph, &c.foreground, &c.background})
                    run.append(record.make_str(*text));
                run.append(record.make_bool(c.bold));
                run.append(record.make_num(c.dim));
                run.append(record.make_num(c.other_attributes));
                run.append(record.make_num(c.hyperlink));
                runs.append(run);
                begin = end;
            }
            entry.add("complete_cell_runs", runs);
            frames.append(entry);
        }
        value.add("five_frames", frames);
        groups.add(group_names[g], value);
    }
    record.add("capture_groups", groups);
}

// Markdown applies inherited dim across its complete allocation, including
// separator and short-row tails. Check that exact public scope cell by cell
// before removing the known bit. The pure geometry checker then checks zero
// dim everywhere; foreground/background/bold/other attributes remain intact.
inline auth::Frame checked_dim(const auth::Frame& actual, bool dim, OracleWork& work) {
    auth::frame_limits(actual, work);
    const int stop = auth::following_sentinel(actual, kSentinel, work);
    source::detail::pay(work, actual.cells.size(), sizeof(auth::ObservedCell) + 64, 1024);
    auto copy = actual;
    for (int y = 0; y < actual.height; ++y)
        for (int x = 0; x < actual.width; ++x) {
            auto& cell = copy.at(x, y);
            need(cell.dim == static_cast<std::uint16_t>(dim && y < stop ? 2 : 0),
                 "physical: ER107 exact inherited Markdown dim allocation");
            cell.dim = 0;
        }
    return copy;
}

struct Checked {
    int width = 0, height = 0, body_width = 0, body_height = 0;
    bool narrow_source = false;
};

inline Checked validate(const Group& observed, const auth::BoundCase& bound,
                        const auth::Surfaces& colors, bool dim, OracleWork& work) {
    need(observed.captures == 5, "protocol: ER107 five complete public states");
    const auto& f = observed.frames;
    need(f[0] == f[2], "physical: ER107 restored full frame including attributes");
    need(f[0] != f[3], "physical: ER107 original still ordinary source fallback");
    auto first = checked_dim(f[0], dim, work);
    auto scene = auth::check_complete_frame(first, bound, colors, kSentinel, work);
    Checked checked{scene->intrinsic_width, scene->total_rows, scene->body_width,
                    scene->body_height, scene->intrinsic_width > 40};
    auto restored = checked_dim(f[2], dim, work);
    static_cast<void>(auth::check_complete_frame(restored, bound, colors, kSentinel, work));
    if (checked.narrow_source)
        need(f[1] == f[4],
             "physical: ER107 narrow exact ordinary source glyph/FG/BG/bold/dim planes");
    else {
        auto narrow = checked_dim(f[1], dim, work);
        static_cast<void>(auth::check_complete_frame(narrow, bound, colors, kSentinel, work));
    }
    return checked;
}

inline void metadata(json::JsonMutVal value, std::size_t index) {
    need(index < kPins.size(), "source: ER107 metadata compiled index");
    const auto& s = kPins[index];
    const auto& t = auth::kPins[index];
    value.set("contract", kContract);
    value.set("path", s.path);
    value.set("source_case_ordinal", static_cast<std::int64_t>(index));
    value.set("manifest_global_ordinal", s.global);
    value.set("prior_ER206_ordinal", s.prior_ordinal);
    value.set("source_bytes", static_cast<std::int64_t>(s.raw_bytes));
    value.set("source_sha256", s.raw_sha);
    value.set("source_git_blob", s.git_blob);
    value.set("source_case_sha256", s.case_sha);
    value.set("source_model_sha256", s.model_sha);
    value.set("source_witnesses_sha256", s.witness_sha);
    value.set("manifest_record_sha256", s.manifest_sha);
    value.set("prior_ER206_case_sha256", s.prior_case_sha);
    value.set("metadata_sha256", s.metadata_sha);
    value.set("config_obligations_sha256", s.config_obligations_sha);
    value.set("projection_case_sha256", t.case_sha);
    value.set("terminal_model_sha256", t.model_sha);
    value.set("whole_scene_model_sha256", t.scene_sha);
    value.set("source_binding_sha256", t.binding_sha);
    value.set("complete_model_sha256", t.complete_sha);
    value.set("source_facts_sha256", kFactsSha);
    value.set("source_lock_sha256", kSourceLock);
    value.set("manifest_sha256", kManifestSha);
    value.set("prior_authority_sha256", kPriorSha);
    value.set("projection_facts_sha256", auth::kProjectionSha);
    value.set("presentation_facts_sha256", auth::kPresentationSha);
    value.set("contract_sha256", auth::kContractSnapshotSha);
    value.set("source_helper_sha256", kSourceHelperSha);
    value.set("authority_adaptation", auth::kAdaptation);
    value.set("authority_review_status", auth::kB3ReviewStatus);
    value.set("limitations", auth::kB3Limitations);
    value.set("foreign_assertions_certified", 0);
    value.set("native_report_input", false);
    value.set("drawing_credit", 0);
    value.set("formal_admission", false);
    value.set("join_required", true);
}

struct Result {
    std::string proof;
    bool ready = false, captured = false, bound = false, published = false, resource = false;
    std::size_t semantic = 0, physical = 0;
};

inline Result run(const Documents& documents, std::size_t index, std::ostream& diagnostics) {
    need(index < kPins.size(), "source: ER107 Official source index");
    json::JsonMutDoc report, compact;
    auto record = report.object(), proof = compact.object();
    metadata(record, index);
    metadata(proof, index);
    Captures captured;
    Result result;
    Checked checked;
    OracleWork capture_work, binding_work, publication_work, controls;
    std::array<OracleWork, 3> checks;
    std::string error, phase = "capture";
    std::vector<std::string> executed, skipped;
    try {
        const auto raw = read(fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/cases" /
                                  std::string(kPins[index].path),
                              kMaxRawBytes);
        observe(captured, raw, capture_work);
        result.captured = captured.complete();
        // Publication of all concrete cells is also ordered before typed bind.
        publish(captured, record, publication_work);
        result.published = true;
        phase = "bind";
        const auto bound = documents.bind(index, raw, captured, binding_work);
        result.bound = true;
        record.add("original_manifest_expectation",
                   report.copy_val(bound.source.manifest_record.get("raw")));
        record.set("original_foreign_obligations",
                   "Retained by frozen ER206, B1 source Model and presentation/terminal Models; "
                   "browser RGB/CSS/SVG/font assertions are not certified.");
        const auto colors = surfaces();
        phase = "complete_scene";
        checked = validate(captured.groups[0], bound, colors, true, checks[0]);
        static_cast<void>(validate(captured.groups[1], bound, colors, true, checks[1]));
        static_cast<void>(validate(captured.groups[2], bound, colors, true, checks[2]));
        need(captured.groups[0].frames == captured.groups[1].frames &&
                 captured.groups[1].frames == captured.groups[2].frames,
             "physical: ER107 uncached/cache miss/cache hit all fifteen complete frames");
        phase = "good_first_controls";
        auto normalized = checked_dim(captured.groups[0].frames[0], true, controls);
        const auto mutations = auth::exercise(normalized, bound, colors, kSentinel, controls);
        result.semantic = mutations.semantic;
        result.physical = mutations.physical;
        executed = mutations.executed;
        skipped = mutations.skipped;
        source::detail::pay(controls, captured.groups[0].frames[0].cells.size(),
                            sizeof(auth::ObservedCell) + 64, 1024);
        auto bad_dim = captured.groups[0].frames[0];
        bad_dim.at(0, 0).dim ^= 2;
        bool rejected = false;
        try {
            static_cast<void>(checked_dim(bad_dim, true, controls));
        } catch (const std::exception& failure) {
            if (auth::resource(failure.what())) throw;
            rejected = true;
        }
        need(rejected, "protocol: ER107 actual inherited dim mutant survived");
        ++result.physical;
        executed.emplace_back("official_markdown_allocation_dim");
        need(result.semantic > 0 && result.physical >= 7,
             "protocol: ER107 meaningful source and full-style negatives");
        phase = "complete";
        result.ready = true;
    } catch (const std::bad_alloc&) {
        result.resource = true;
        error = "resource: ER107 Official allocation failed";
    } catch (const std::exception& failure) {
        error = failure.what();
        if (error.size() > 1024) error.resize(1024);
        result.resource = auth::resource(error);
    }
    result.captured = captured.complete();
    // Failed partial acquisitions still retain every frame actually obtained;
    // no complete fifteen-frame publication is claimed on a serialization error.
    if (!result.published) try {
            publish(captured, record, publication_work);
            result.published = true;
        } catch (const std::bad_alloc&) {
            if (error.empty()) error = "resource: ER107 Official evidence allocation failed";
            result.resource = true;
            result.ready = false;
        } catch (const std::exception& failure) {
            if (error.empty()) error = failure.what();
            result.resource = result.resource || auth::resource(failure.what());
            result.ready = false;
        }
    for (auto value : {record, proof}) {
        value.set("local_ready", result.ready);
        value.set("captures", static_cast<std::int64_t>(captured.count()));
        value.set("captures_before_binding", result.bound && result.captured);
        value.set("bound_after_captures", result.bound);
        value.set("full_frames_published", result.published && result.captured);
        value.set("geometry_and_restoration_good", result.ready);
        value.set("narrow_exact_ordinary", result.ready && checked.narrow_source);
        value.set("narrow_geometry_good", result.ready && !checked.narrow_source);
        value.set("uncached_cached_five_planes_equal", result.ready);
        value.set("cache_hit_observed", captured.cache_hit);
        value.set("markdown_dim_color", true);
        value.set("inherited_other_attributes", 2);
        value.set("dim_normalization",
                  "exact allocation bit checked before zero-dim complete-scene validation");
        value.set("width", checked.width);
        value.set("height", checked.height);
        value.set("body_width", checked.body_width);
        value.set("body_height", checked.body_height);
        value.set("semantic_mutations_rejected", static_cast<std::int64_t>(result.semantic));
        value.set("physical_mutations_rejected", static_cast<std::int64_t>(result.physical));
        value.set("verification_resource_exhausted", result.resource);
        value.set("phase", phase);
        value.set("verification_phase_count", 4);
        value.set("verification_phase_cap", 512000000);
        value.set("verification_aggregate_cap", static_cast<std::int64_t>(2048000000));
        auto done = value.make_arr();
        for (const auto& id : executed)
            done.append(value.make_str(id));
        value.add("mutations_executed", done);
        auto omitted = value.make_arr();
        for (const auto& id : skipped)
            omitted.append(value.make_str(id));
        value.add("mutations_inapplicable", omitted);
        if (!error.empty()) value.set("error", error);
    }
    report.set_root(record);
    diagnostics << report.to_string() << '\n';
    need(diagnostics.good(), "resource: ER107 Official full-frame evidence output failed");
    compact.set_root(proof);
    result.proof = compact.to_string();
    return result;
}

inline bool owned(JsonVal proof) {
    const auto ordinal = proof.get("source_case_ordinal");
    need(ordinal.is_int() && ordinal.as_int() >= 0 && ordinal.as_int() < 107,
         "source: ER107 owned source ordinal");
    const auto index = static_cast<std::size_t>(ordinal.as_int());
    json::JsonMutDoc expected;
    auto root = expected.object();
    metadata(root, index);
    expected.set_root(root);
    const auto parsed = json::parse(expected.to_string());
    need(parsed.has_value(), "protocol: ER107 owned metadata JSON");
    OracleWork pins;
    parsed->root().iter_obj([&](JsonVal key, JsonVal value) {
        auth::exact(proof.get(key.as_str()), value, pins, "source: ER107 owned literal proof pin");
    });
    const std::set<std::string> runtime{"local_ready",
                                        "captures",
                                        "captures_before_binding",
                                        "bound_after_captures",
                                        "full_frames_published",
                                        "geometry_and_restoration_good",
                                        "narrow_exact_ordinary",
                                        "narrow_geometry_good",
                                        "uncached_cached_five_planes_equal",
                                        "cache_hit_observed",
                                        "markdown_dim_color",
                                        "inherited_other_attributes",
                                        "dim_normalization",
                                        "width",
                                        "height",
                                        "body_width",
                                        "body_height",
                                        "semantic_mutations_rejected",
                                        "physical_mutations_rejected",
                                        "verification_resource_exhausted",
                                        "phase",
                                        "verification_phase_count",
                                        "verification_phase_cap",
                                        "verification_aggregate_cap",
                                        "mutations_executed",
                                        "mutations_inapplicable",
                                        "error"};
    proof.iter_obj([&](JsonVal key, JsonVal) {
        need(parsed->root().has(key.as_str()) || runtime.contains(std::string(key.as_str())),
             "source: ER107 closed Official case proof");
    });
    const auto yes = [&](std::string_view key) {
        const auto v = proof.get(key);
        need(v.is_bool(), "source: ER107 Official proof boolean");
        return v.as_bool();
    };
    if (!yes("local_ready")) {
        need(proof.get("error").is_str() && !proof.get_string("error").empty(),
             "source: ER107 failed proof has explicit error");
        return false;
    }
    need(!proof.has("error") && !yes("verification_resource_exhausted"),
         "source: ER107 good proof has no error/resource");
    need(proof.get("captures").is_int() && proof.get_int("captures") == 15,
         "source: ER107 fifteen actual public frames");
    for (const auto key :
         {"captures_before_binding", "bound_after_captures", "full_frames_published",
          "geometry_and_restoration_good", "uncached_cached_five_planes_equal",
          "cache_hit_observed", "markdown_dim_color"})
        need(yes(key), "source: ER107 complete geometry/capture/cache responsibility");
    need(proof.get("inherited_other_attributes").is_int() &&
             proof.get_int("inherited_other_attributes") == 2 &&
             proof.get_string("phase") == "complete",
         "source: ER107 exact inherited dim and completed verification");
    for (const auto& [key, value] : std::array<std::pair<std::string_view, std::int64_t>, 3>{
             {{"verification_phase_count", 4},
              {"verification_phase_cap", 512000000},
              {"verification_aggregate_cap", 2048000000}}})
        need(proof.get(key).is_int() && proof.get_int(key) == value,
             "source: ER107 fixed independent checking phase bounds");
    for (const auto key : {"width", "height", "body_width", "body_height"})
        need(proof.get(key).is_int() && proof.get_int(key) > 0 &&
                 proof.get_int(key) <=
                     (std::string_view(key).find("width") != std::string_view::npos ? 240 : 1024),
             "source: ER107 complete extent bounds");
    need(yes("narrow_exact_ordinary") != yes("narrow_geometry_good") &&
             yes("narrow_exact_ordinary") == (proof.get_int("width") > 40),
         "source: ER107 width/narrow XOR");
    need(proof.get("semantic_mutations_rejected").is_int() &&
             proof.get_int("semantic_mutations_rejected") > 0 &&
             proof.get("physical_mutations_rejected").is_int() &&
             proof.get_int("physical_mutations_rejected") >= 7,
         "source: ER107 meaningful semantic/physical controls");
    const auto executed = proof.get("mutations_executed"),
               skipped = proof.get("mutations_inapplicable");
    need(executed.is_arr() && skipped.is_arr() &&
             executed.size() ==
                 static_cast<std::size_t>(proof.get_int("semantic_mutations_rejected") +
                                          proof.get_int("physical_mutations_rejected")),
         "source: ER107 complete executed mutation identities");
    std::set<std::string> ids;
    for (std::size_t i = 0; i < executed.size(); ++i) {
        const auto id = executed.at(i);
        need(id.is_str() && ids.insert(std::string(id.as_str())).second,
             "source: ER107 unique executed good-first control");
    }
    need(ids.contains("official_markdown_allocation_dim"),
         "source: ER107 full Markdown dim negative");
    return true;
}
}  // namespace loom::test::mermaid_er107::official
