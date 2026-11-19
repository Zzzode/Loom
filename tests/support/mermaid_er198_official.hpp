// Official-owned RFC 0034 observations for the one-row ER198 cohort. The frozen
// one-row proposal (increment 2a) supplies expectations; producer code and
// Native evidence are not inputs. No matrix admission precedes an owned,
// reviewed proof. Structurally mirrors mermaid_er_groupedge_official.hpp but is
// bounded to a single projected case and the ER198/complete-original/v1
// contract.
//
// Footer-aware geometry (the one divergence from the non-projection group-edge
// cohort): a reviewed ER projection appends a production notice block below the
// diagram that is not part of the typed whole_scene_model. The
// er_projection_notices_json_for_testing seam is the authoritative source of
// those rows; we bind the rendered footer to it, then run the strict
// full-claim validate_impl over the diagram-only subgrid. The test-appended
// following sentinel stays handled by following_row exactly as in group-edge.
#pragma once
#include "mermaid_er198_source.hpp"
#include "mermaid_er206_authority.hpp"
#include "mermaid_er_groupedge_official.hpp"

namespace loom::test::mermaid_er198::official {
namespace source = loom::test::mermaid_er198;
namespace rel = loom::test::mermaid_relations;
namespace json = loom::utils::json;
namespace mtest = loom::ui::mermaid::testing;
namespace fs = std::filesystem;
// The mutation control closed set is ER206-wide and identical across cohorts;
// reuse the RFC 0033 vocabulary rather than fork the string literals.
namespace ge_official = loom::test::mermaid_er_groupedge::official;

inline constexpr std::string_view kContract = "ER198/complete-original/v1";
inline constexpr std::string_view kSentinel = rel::kRelationsFollowingToken;
inline constexpr std::string_view kEvidence = "mermaid-er198-official.jsonl";
using Frames = std::array<rel::Grid, 5>;

inline void need(bool good, std::string_view message) {
    rel::class481_require(good, message);
}

inline std::string hash(std::string_view raw) {
    return loom::utils::crypto::sha256(raw);
}

inline std::string read(const fs::path& path, std::size_t cap = source::kMaxRawBytes) {
    need(!fs::is_symlink(fs::symlink_status(path)) && fs::is_regular_file(path),
         "source: ER198 regular input");
    need(fs::file_size(path) <= cap, "resource: ER198 input byte bound");
    std::ifstream stream(path, std::ios::binary);
    need(stream.good(), "source: ER198 input open");
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
        for (const auto& group : groups) n += group.captures;
        return n;
    }

    bool complete() const {
        return count() == 15 &&
               std::ranges::all_of(groups, [](const Group& group) { return group.captures == 5; });
    }
};

struct Documents {
    json::JsonDoc proposal, manifest, prior;

    Documents() {
        const fs::path fixtures = fs::path(LOOM_TESTS_DIR) / "fixtures";
        const auto proposal_text = read(fixtures / std::string(source::kProposalPath),
                                        4u * 1024u * 1024u);
        const auto manifest_text =
            read(fixtures / "mermaid_official/manifest.json", 4u * 1024u * 1024u);
        const auto prior_text = read(fixtures / std::string(source::kPriorPath),
                                     4u * 1024u * 1024u);
        need(hash(proposal_text) == source::kProposalSha,
             "source: ER198 frozen proposal hash");
        need(hash(manifest_text) == source::kManifestSha,
             "source: ER198 frozen manifest hash");
        need(hash(prior_text) == source::kPriorSha,
             "source: ER198 frozen ER206 prior hash");
        auto pd = json::parse(proposal_text), md = json::parse(manifest_text),
             od = json::parse(prior_text);
        need(pd && md && od, "source: ER198 immutable document JSON");
        proposal = std::move(*pd);
        manifest = std::move(*md);
        prior = std::move(*od);
        need(proposal.root().get("cases").size() == source::kCohortSize,
             "source: ER198 one-case proposal population");
    }
};

inline std::string fence(std::string_view raw, std::string_view language) {
    std::size_t run = 0, longest = 0;
    for (const char c : raw) {
        run = c == '`' ? run + 1 : 0;
        longest = std::max(longest, run);
    }
    const std::string ticks(std::max(std::size_t{3}, longest + 1), '`');
    return ticks + std::string(language) + "\n" + std::string(raw) + "\n" + ticks +
           "\n\n" + std::string(kSentinel);
}

// One cache-mode group renders the same five public states: mermaid drawing at
// 240/40/240, then the ordinary text control at 240/40. A null cache is the
// uncached group; the shared LayoutRowsCache drives first a miss then a hit.
inline void observe(Group& out, std::string_view raw, rel::Work& work,
                    loom::ui::mermaid::LayoutRowsCache* cache = nullptr) {
    need(out.captures == 0, "protocol: ER198 each group owns fresh observations");
    loom::ui::MarkdownOptions options;
    options.mermaid_rows_cache = cache;
    auto drawing = loom::ui::render_markdown(fence(raw, "mermaid"), options);
    options.mermaid_rows_cache = nullptr;
    auto ordinary = loom::ui::render_markdown(fence(raw, "text"), options);
    drawing->ComputeRequirement();
    ordinary->ComputeRequirement();
    const int height =
        std::max(24, std::max(drawing->requirement().min_y, ordinary->requirement().min_y) + 2);
    need(height <= 1040, "resource: ER198 public requirements exceed capture bound");
    constexpr std::array<int, 5> widths{240, 40, 240, 240, 40};
    for (std::size_t i = 0; i < 5; ++i) {
        out.frames[i] =
            rel::note13_capture(i < 3 ? drawing : ordinary, widths[i], height, work);
        ++out.captures;
    }
}

inline void observe(Captures& out, std::string_view raw, rel::Work& work) {
    need(raw.find(kSentinel) == std::string_view::npos,
         "source: ER198 raw source/sentinel collision");
    loom::ui::mermaid::LayoutRowsCache cache(4, 4 * 1024 * 1024);
    observe(out.groups[0], raw, work);
    observe(out.groups[1], raw, work, &cache);
    observe(out.groups[2], raw, work, &cache);
    out.cache_hit = cache.hits() > 0;
    need(out.cache_hit, "protocol: ER198 actual public Markdown cache hit");
}

inline std::string cell_digest(const rel::Grid& frame) {
    std::string raw;
    raw.reserve(frame.cells.size() * 8u);
    for (const auto& cell : frame.cells) {
        raw += std::to_string(cell.size());
        raw += ':';
        raw += cell;
        raw += ';';
    }
    return hash(raw);
}

struct Checked {
    int width = 0, height = 0, body_width = 0, body_height = 0;
    bool narrow_source = false;
};

struct Result {
    std::string proof;
    bool ready = false, captured = false, bound = false, resource = false;
    std::size_t semantic = 0, physical = 0;
};

inline void metadata(json::JsonMutVal value, std::size_t index) {
    need(index < source::kPins.size(), "source: ER198 metadata compiled index");
    const auto& s = source::kPins[index];
    value.set("contract", kContract);
    value.set("path", s.path);
    value.set("source_case_ordinal", static_cast<int64_t>(index));
    value.set("manifest_global_ordinal", s.global);
    value.set("prior_ER206_ordinal", s.prior_ordinal);
    value.set("source_bytes", static_cast<int64_t>(s.raw_bytes));
    value.set("source_sha256", s.raw_sha);
    value.set("whole_scene_model_sha256", s.whole_scene_sha);
    value.set("source_model_sha256", s.model_sha);
    value.set("source_witnesses_sha256", s.witness_sha);
    value.set("source_binding_sha256", s.binding_sha);
    value.set("source_asset_case_sha256", s.asset_case_sha);
    value.set("manifest_record_sha256", s.manifest_record_sha);
    value.set("prior_ER206_case_canonical_sha256", s.prior_case_sha);
    value.set("proposal_sha256", source::kProposalSha);
    value.set("manifest_sha256", source::kManifestSha);
    value.set("prior_authority_sha256", source::kPriorSha);
    value.set("review_status", "PENDING_INDEPENDENT_REVIEW");
    value.set("foreign_assertions_certified", 0);
    value.set("formal_admission", false);
    value.set("drawing_credit", 0);
}

// Trimmed, non-space content of one grid row, used to match production notices.
inline std::string row_content(const rel::Grid& grid, int y) {
    std::string out;
    for (int x = 0; x < grid.width; ++x) {
        const auto& cell = grid.cells[static_cast<std::size_t>(grid.index({x, y}))];
        out += cell;
    }
    const auto first = out.find_first_not_of(' ');
    if (first == std::string::npos) return {};
    const auto last = out.find_last_not_of(' ');
    return out.substr(first, last - first + 1);
}

// Locate the diagram/footer boundary from the authoritative notice seam and
// bind the rendered footer rows to it. Returns the first footer row index; the
// returned subgrid covers only diagram rows [0, footer_start). For a
// non-projection source (no notices) the whole frame is the diagram region.
struct FooterBoundary {
    int footer_start;
    std::size_t notice_count;
};

inline rel::Grid diagram_subgrid(const rel::Grid& frame, std::string_view raw,
                                 FooterBoundary& boundary) {
    mtest::Work seam_work;
    const auto notice_text = mtest::er_projection_notices_json_for_testing(raw, seam_work);
    need(notice_text.has_value(), "source: ER198 projection notice seam");
    const auto parsed = json::parse(*notice_text);
    need(parsed.has_value(), "source: ER198 notice seam JSON");
    const auto root = parsed->root();
    const auto notice_rows = root.get("notice_rows");
    boundary.notice_count = static_cast<std::size_t>(notice_rows.size());
    // The projection flag and the populated notice block must agree, so the
    // seam's has_projection field is actually consumed rather than decorative.
    need(root.get("has_projection").as_bool() == (boundary.notice_count > 0),
         "source: ER198 projection flag/notice block consistency");
    if (boundary.notice_count == 0) {
        boundary.footer_start = frame.height;
        return frame;
    }
    // Data-driven (not string-literal) boundary: the first frame row equal to
    // the seam's first notice. The seam is production-derived, so the marker
    // moves with any renderer change instead of drifting silently.
    int footer_start = -1;
    for (int y = 0; y < frame.height; ++y)
        if (row_content(frame, y) == notice_rows.at(0).as_str()) {
            footer_start = y;
            break;
        }
    need(footer_start >= 0, "source: ER198 rendered footer start not found");
    // Bind every footer row to the production notice set, in order.
    need(footer_start + static_cast<int>(boundary.notice_count) <= frame.height,
         "source: ER198 footer extent within frame bounds");
    for (std::size_t i = 0; i < boundary.notice_count; ++i)
        need(row_content(frame, footer_start + static_cast<int>(i)) ==
                 notice_rows.at(i).as_str(),
             "source: ER198 footer row matches production notice");
    boundary.footer_start = footer_start;
    rel::Grid sub;
    sub.width = frame.width;
    sub.height = footer_start;
    sub.cells.reserve(static_cast<std::size_t>(frame.width) * footer_start);
    for (int y = 0; y < footer_start; ++y)
        for (int x = 0; x < frame.width; ++x)
            sub.cells.push_back(frame.cells[static_cast<std::size_t>(frame.index({x, y}))]);
    return sub;
}

inline Result run(const Documents& documents, std::size_t index, std::ostream& diagnostics) {
    need(index < source::kPins.size(), "source: ER198 Official source index");
    json::JsonMutDoc report, compact;
    auto record = report.object(), proof = compact.object();
    metadata(record, index);
    metadata(proof, index);
    Captures captured;
    Result result;
    Checked checked;
    rel::Work capture_work{rel::kEr206SceneLimit}, binding_work{rel::kEr206SceneLimit},
        geometry_work{rel::kEr206SceneLimit}, mutation_work{rel::kEr206SceneLimit};
    std::string error, phase = "capture";
    std::vector<std::string> executed, skipped;
    FooterBoundary boundary{};
    try {
        const auto& pin = source::kPins[index];
        const auto raw =
            read(fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/cases" /
                     std::string(pin.path));
        observe(captured, raw, capture_work);
        result.captured = captured.complete();
        auto frame_groups = record.make_obj();
        constexpr std::array<std::string_view, 3> group_names{
            "uncached", "cache_miss", "cache_hit"};
        for (std::size_t g = 0; g < captured.groups.size(); ++g) {
            auto gobj = frame_groups.make_obj();
            auto farr = gobj.make_arr();
            for (std::size_t n = 0; n < captured.groups[g].captures; ++n) {
                auto entry = farr.make_obj();
                entry.set("state_n", static_cast<int64_t>(n));
                entry.set("cell_sha256", cell_digest(captured.groups[g].frames[n]));
                farr.append(entry);
            }
            gobj.add("five_frames", farr);
            frame_groups.add(group_names[g], gobj);
        }
        record.add("capture_groups", frame_groups);
        phase = "bind";
        const auto case_value = [&] {
            std::optional<json::JsonVal> found;
            documents.proposal.root().get("cases").iter([&](json::JsonVal c) {
                if (c.get_string("path") == pin.path) found = c;
            });
            need(found.has_value(), "source: ER198 proposal case binding");
            return *found;
        }();
        source::verify_case(index, case_value, raw, binding_work);
        result.bound = true;
        rel::Model model =
            rel::er296_typed_model(case_value.get("whole_scene_model"), binding_work);
        phase = "complete_scene";
        const auto& f = captured.groups[0].frames;
        // Three cache modes must produce byte-identical planes.
        for (std::size_t g = 1; g < captured.groups.size(); ++g)
            need(captured.groups[g].frames == f,
                 "physical: ER198 cache modes share all five planes");
        const bool restored = (f[0] == f[2]);
        const bool wide_source = (f[0] == f[3]);
        const bool narrow_source = (f[1] == f[4]);
        // following-row a/b/c run on the full sentinel-terminated frames; the
        // oracle already skips the test-appended following sentinel row.
        const bool a = rel::following_row(f[0], geometry_work, kSentinel).has_value();
        const bool b = rel::following_row(f[1], geometry_work, kSentinel).has_value();
        const bool c = rel::following_row(f[2], geometry_work, kSentinel).has_value();
        // Strict full-claim geometry runs over the diagram-only subgrid; the
        // projection notice footer is separately bound to the production seam.
        const auto wide_sub = diagram_subgrid(f[0], raw, boundary);
        const auto geo_errors =
            rel::validate_impl(wide_sub, model, geometry_work, false, nullptr, false, nullptr,
                               kSentinel);
        need(geo_errors.empty(),
             geo_errors.empty() ? "" : ("geometry: ER198 complete frame " + geo_errors.front()));
        const auto boxes = rel::outlines(wide_sub, geometry_work);
        need(boxes.size() > model.groups.size(),
             "geometry: ER198 entity and group contours detected");
        int body_right = 0, body_bottom = 0;
        for (const auto& rect : boxes) {
            body_right = std::max(body_right, rect.r);
            body_bottom = std::max(body_bottom, rect.b);
        }
        checked = {f[0].width, f[0].height, body_right + 1, body_bottom + 1,
                   narrow_source && b};
        need(restored && a && b && c && !wide_source,
             "geometry: ER198 restored/wide/narrow following-row contract");
        phase = "mutations";
        const auto model_bytes =
            rel::class481_canonical(case_value.get("whole_scene_model"), binding_work).size();
        const auto changes =
            rel::er206_mutations(wide_sub, model, model_bytes, mutation_work, kSentinel);
        result.physical = changes.executed.count("orphan_ink")
                              ? static_cast<std::size_t>(changes.executed.at("orphan_ink"))
                              : 0;
        std::size_t total_executed = 0;
        for (const auto& [id, n] : changes.executed) {
            total_executed += static_cast<std::size_t>(n);
            executed.push_back(id);
        }
        for (const auto& [id, n] : changes.skipped) skipped.push_back(id);
        result.semantic = total_executed - result.physical;
        std::ranges::sort(executed);
        executed.erase(std::unique(executed.begin(), executed.end()), executed.end());
        std::ranges::sort(skipped);
        skipped.erase(std::unique(skipped.begin(), skipped.end()), skipped.end());
        need(changes.executed.count("relation_label") &&
                 changes.executed.count("start_cardinality") &&
                 changes.executed.count("end_cardinality"),
             "protocol: ER198 group-edge label/cardinality controls present");
        need(changes.executed.count("group_caption"),
             "protocol: ER198 group caption control present");
        const bool has_nested =
            std::ranges::any_of(model.groups, [](const rel::Group& g) { return !g.parent.empty(); });
        need(has_nested == static_cast<bool>(changes.executed.count("group_parent")),
             "protocol: ER198 group_parent control matches nested group tree");
        need(result.semantic > 0, "protocol: ER198 meaningful semantic negatives");
        phase = "complete";
        result.ready = true;
    } catch (const std::bad_alloc&) {
        result.resource = true;
        error = "resource: ER198 Official allocation failed";
    } catch (const std::exception& failure) {
        error = failure.what();
        if (error.size() > 1024) error.resize(1024);
    }
    result.captured = captured.complete();
    for (auto value : {record, proof}) {
        value.set("local_ready", result.ready);
        value.set("captures", static_cast<int64_t>(captured.count()));
        value.set("captures_before_binding", result.bound && result.captured);
        value.set("bound_after_captures", result.bound);
        value.set("full_frames_published", result.captured);
        value.set("geometry_and_restoration_good", result.ready);
        value.set("narrow_exact_ordinary", result.ready && checked.narrow_source);
        value.set("narrow_geometry_good", result.ready && !checked.narrow_source);
        value.set("uncached_cached_five_planes_equal", result.ready);
        value.set("cache_hit_observed", captured.cache_hit);
        value.set("width", checked.width);
        value.set("height", checked.height);
        value.set("body_width", checked.body_width);
        value.set("body_height", checked.body_height);
        value.set("semantic_mutations_rejected", static_cast<int64_t>(result.semantic));
        value.set("physical_mutations_rejected", static_cast<int64_t>(result.physical));
        value.set("verification_resource_exhausted", result.resource);
        value.set("phase", phase);
        value.set("projection_notice_count",
                  static_cast<int64_t>(boundary.notice_count));
        value.set("diagram_footer_start_row",
                  static_cast<int64_t>(boundary.footer_start));
        auto done = value.make_arr();
        for (const auto& id : executed) done.append(value.make_str(id));
        value.add("mutations_executed", done);
        auto omitted = value.make_arr();
        for (const auto& id : skipped) omitted.append(value.make_str(id));
        value.add("mutations_inapplicable", omitted);
        if (!error.empty()) value.set("error", error);
    }
    report.set_root(record);
    diagnostics << report.to_string() << '\n';
    need(diagnostics.good(), "resource: ER198 evidence output failed");
    compact.set_root(proof);
    result.proof = compact.to_string();
    return result;
}

inline bool owned(json::JsonVal proof) {
    const auto ordinal = proof.get("source_case_ordinal");
    need(ordinal.is_int() && ordinal.as_int() >= 0 &&
             ordinal.as_int() < static_cast<int>(source::kCohortSize),
         "source: ER198 owned source ordinal range");
    const auto index = static_cast<std::size_t>(ordinal.as_int());
    json::JsonMutDoc expected;
    auto root = expected.object();
    metadata(root, index);
    expected.set_root(root);
    const auto parsed = json::parse(expected.to_string());
    need(parsed.has_value(), "protocol: ER198 owned metadata JSON");
    rel::Work pins;
    parsed->root().iter_obj([&](json::JsonVal key, json::JsonVal value) {
        rel::class481_require(
            rel::class481_canonical(proof.get(key.as_str()), pins) ==
                rel::class481_canonical(value, pins),
            "source: ER198 owned literal proof pin");
    });
    const std::set<std::string> runtime{
        "local_ready", "captures", "captures_before_binding", "bound_after_captures",
        "full_frames_published", "geometry_and_restoration_good", "narrow_exact_ordinary",
        "narrow_geometry_good", "uncached_cached_five_planes_equal", "cache_hit_observed",
        "width", "height", "body_width", "body_height", "semantic_mutations_rejected",
        "physical_mutations_rejected", "verification_resource_exhausted", "phase",
        "projection_notice_count", "diagram_footer_start_row",
        "mutations_executed", "mutations_inapplicable", "error"};
    proof.iter_obj([&](json::JsonVal key, json::JsonVal) {
        need(parsed->root().has(key.as_str()) || runtime.contains(std::string(key.as_str())),
             "source: ER198 closed Official case proof");
    });
    const auto yes = [&](std::string_view key) {
        const auto v = proof.get(key);
        need(v.is_bool(), "source: ER198 Official proof boolean");
        return v.as_bool();
    };
    if (!yes("local_ready")) {
        need(proof.get("error").is_str() && !proof.get_string("error").empty(),
             "source: ER198 failed proof has explicit error");
        return false;
    }
    need(!proof.has("error") && !yes("verification_resource_exhausted"),
         "source: ER198 good proof has no error/resource");
    // A ready projected ER198 proof must carry the bound production footer; a
    // silently vanished notice block must not pass as owned.
    need(proof.get("projection_notice_count").is_int() &&
             proof.get_int("projection_notice_count") >= 1,
         "source: ER198 ready proof carries the projection footer");
    need(proof.get("captures").is_int() && proof.get_int("captures") == 15,
         "source: ER198 fifteen actual public frames");
    for (const auto key : {"captures_before_binding", "bound_after_captures",
                           "full_frames_published", "geometry_and_restoration_good",
                           "uncached_cached_five_planes_equal", "cache_hit_observed"})
        need(yes(key), "source: ER198 capture/bind/cache responsibility");
    need(proof.get_string("phase") == "complete",
         "source: ER198 completed verification phase");
    for (const auto key : {"height", "body_width", "body_height"})
        need(proof.get(key).is_int() && proof.get_int(key) > 0 &&
                 proof.get_int(key) <= 1040,
             "source: ER198 complete extent bounds");
    need(proof.get("width").is_int() && proof.get_int("width") == 240,
         "source: ER198 fixed 240-column capture width");
    need(yes("narrow_exact_ordinary") != yes("narrow_geometry_good"),
         "source: ER198 narrow exact-ordinary/full-geometry XOR");
    need(proof.get("semantic_mutations_rejected").is_int() &&
             proof.get_int("semantic_mutations_rejected") > 0 &&
             proof.get("physical_mutations_rejected").is_int() &&
             proof.get_int("physical_mutations_rejected") >= 1,
         "source: ER198 meaningful semantic controls and physical orphan");
    const auto executed = proof.get("mutations_executed"),
               skipped = proof.get("mutations_inapplicable");
    need(executed.is_arr() && skipped.is_arr(),
         "source: ER198 mutation identity arrays");
    std::set<std::string> ids;
    for (std::size_t i = 0; i < executed.size(); ++i) {
        const auto id = executed.at(i);
        const std::string name(id.as_str());
        need(id.is_str() && ids.insert(name).second &&
                 ge_official::executed_vocabulary().contains(name),
             "source: ER198 unique executed control in closed vocabulary");
    }
    std::set<std::string> skip_ids;
    for (std::size_t i = 0; i < skipped.size(); ++i) {
        const auto id = skipped.at(i);
        const std::string name(id.as_str());
        need(id.is_str() && skip_ids.insert(name).second &&
                 ge_official::skipped_vocabulary().contains(name),
             "source: ER198 unique skipped control in closed vocabulary");
    }
    need(ids.contains("relation_label") && ids.contains("start_cardinality") &&
             ids.contains("end_cardinality") && ids.contains("group_caption"),
         "source: ER198 cohort-defining controls present");
    return true;
}
}  // namespace loom::test::mermaid_er198::official
