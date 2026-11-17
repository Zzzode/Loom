// Official-owned RFC 0033 ER group-edge observations for the six-case cohort.
// Frozen source/geometry authority (the checked-in proposal) supplies
// expectations; producer code and Native evidence are not inputs. No matrix
// admission precedes an owned, reviewed proof. Structurally mirrors
// mermaid_er107_official.hpp but is bounded to six cases and the
// ERGroupEdge/complete-original/v1 contract, and builds its capture/geometry/
// mutation rigor on the sha-frozen relations oracle rather than a new renderer
// authority. The ER107 helpers are included/reused, never edited.
#pragma once
#include "mermaid_er_groupedge_source.hpp"
#include "mermaid_er206_authority.hpp"

namespace loom::test::mermaid_er_groupedge::official {
namespace source = loom::test::mermaid_er_groupedge;
namespace rel = loom::test::mermaid_relations;
namespace json = loom::utils::json;
namespace fs = std::filesystem;

inline constexpr std::string_view kContract = "ERGroupEdge/complete-original/v1";
inline constexpr std::string_view kSentinel = rel::kRelationsFollowingToken;
inline constexpr std::string_view kEvidence = "mermaid-er-groupedge-official.jsonl";
// Byte-drift tripwire for the sibling source helper. This C++ pin is recomputed
// from the helper bytes and guards accidental edits only; it is NOT the
// independent review receipt. RFC 0033 review authority is enforced separately
// by tools/mermaid/verify_er_groupedge_admission.py, whose REVIEWED_HELPER_SHA
// map ships empty and fails closed until an independent review fills it.
inline constexpr std::string_view kSourceHelperSha =
    "2778b1fb896b8562f5fceadb0c27b8c516fedac6908adc4dbdd8a3b0d87766fc";
using Frames = std::array<rel::Grid, 5>;

// Closed mutation vocabulary of the reviewed relations oracle
// (er206_mutations_core reject kinds plus the physical orphan_ink negative).
// An owned proof may only name controls from these sets; an invented id is
// rejected even when every other gate passes. The Python verifier additionally
// pins the exact per-case identity set, this C++ gate enforces the closed set.
inline const std::set<std::string>& executed_vocabulary() {
    static const std::set<std::string> kIds{
        "attribute_comment", "attribute_erasure", "attribute_key_count", "attribute_keys",
        "attribute_name", "attribute_order", "attribute_type", "comment_br_merge",
        "comment_br_shift", "comment_line_erasure", "complete_header", "end_cardinality",
        "group_caption", "group_membership", "group_parent", "isolated_entity_erasure",
        "ordered_keys", "orphan_ink", "relation_br_merge", "relation_br_shift",
        "relation_erasure", "relation_label", "relation_line_erasure", "relation_line_order",
        "start_cardinality", "stroke", "third_endpoint"};
    return kIds;
}

inline const std::set<std::string>& skipped_vocabulary() {
    static const std::set<std::string> kIds{
        "identical_duplicate_not_independent_ink",
        "incident_entity_needs_coherent_graph_removal", "no_distinct_ordered_key_pair",
        "no_third_entity", "root_entity_has_no_membership_to_erase",
        "root_group_has_no_parent_to_erase"};
    return kIds;
}

inline void need(bool good, std::string_view message) {
    rel::class481_require(good, message);
}

inline std::string hash(std::string_view raw) {
    return loom::utils::crypto::sha256(raw);
}

inline std::string read(const fs::path& path, std::size_t cap = source::kMaxRawBytes) {
    need(!fs::is_symlink(fs::symlink_status(path)) && fs::is_regular_file(path),
         "source: ERGroupEdge regular input");
    need(fs::file_size(path) <= cap, "resource: ERGroupEdge input byte bound");
    std::ifstream stream(path, std::ios::binary);
    need(stream.good(), "source: ERGroupEdge input open");
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
             "source: ERGroupEdge frozen proposal hash");
        need(hash(manifest_text) == source::kManifestSha,
             "source: ERGroupEdge frozen manifest hash");
        need(hash(prior_text) == source::kPriorSha,
             "source: ERGroupEdge frozen ER206 prior hash");
        need(hash(read(fs::path(LOOM_TESTS_DIR) / "support/mermaid_er_groupedge_source.hpp",
                       262144)) == kSourceHelperSha,
             "source: ERGroupEdge B1 source helper pin");
        auto pd = json::parse(proposal_text), md = json::parse(manifest_text),
             od = json::parse(prior_text);
        need(pd && md && od, "source: ERGroupEdge immutable document JSON");
        proposal = std::move(*pd);
        manifest = std::move(*md);
        prior = std::move(*od);
        need(proposal.root().get("cases").size() == source::kCohortSize,
             "source: ERGroupEdge six-case proposal population");
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
    need(out.captures == 0, "protocol: ERGroupEdge each group owns fresh observations");
    loom::ui::MarkdownOptions options;
    options.mermaid_rows_cache = cache;
    auto drawing = loom::ui::render_markdown(fence(raw, "mermaid"), options);
    options.mermaid_rows_cache = nullptr;
    auto ordinary = loom::ui::render_markdown(fence(raw, "text"), options);
    drawing->ComputeRequirement();
    ordinary->ComputeRequirement();
    const int height =
        std::max(24, std::max(drawing->requirement().min_y, ordinary->requirement().min_y) + 2);
    need(height <= 1040, "resource: ERGroupEdge public requirements exceed capture bound");
    constexpr std::array<int, 5> widths{240, 40, 240, 240, 40};
    for (std::size_t i = 0; i < 5; ++i) {
        out.frames[i] =
            rel::note13_capture(i < 3 ? drawing : ordinary, widths[i], height, work);
        ++out.captures;
    }
}

inline void observe(Captures& out, std::string_view raw, rel::Work& work) {
    need(raw.find(kSentinel) == std::string_view::npos,
         "source: ERGroupEdge raw source/sentinel collision");
    loom::ui::mermaid::LayoutRowsCache cache(4, 4 * 1024 * 1024);
    observe(out.groups[0], raw, work);
    observe(out.groups[1], raw, work, &cache);
    observe(out.groups[2], raw, work, &cache);
    out.cache_hit = cache.hits() > 0;
    need(out.cache_hit, "protocol: ERGroupEdge actual public Markdown cache hit");
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
    need(index < source::kPins.size(), "source: ERGroupEdge metadata compiled index");
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
    value.set("source_helper_sha256", kSourceHelperSha);
    value.set("review_status", "PENDING_INDEPENDENT_REVIEW");
    value.set("foreign_assertions_certified", 0);
    value.set("formal_admission", false);
    value.set("drawing_credit", 0);
}

inline Result run(const Documents& documents, std::size_t index, std::ostream& diagnostics) {
    need(index < source::kPins.size(), "source: ERGroupEdge Official source index");
    json::JsonMutDoc report, compact;
    auto record = report.object(), proof = compact.object();
    metadata(record, index);
    metadata(proof, index);
    Captures captured;
    Result result;
    Checked checked;
    // Each leg owns a fresh independent budget at the established ER206 scene
    // limit; the default 8M Work is only for small oracle accounting.
    rel::Work capture_work{rel::kEr206SceneLimit}, binding_work{rel::kEr206SceneLimit},
        geometry_work{rel::kEr206SceneLimit}, mutation_work{rel::kEr206SceneLimit};
    std::string error, phase = "capture";
    std::vector<std::string> executed, skipped;
    try {
        const auto& pin = source::kPins[index];
        const auto raw =
            read(fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/cases" /
                     std::string(pin.path));
        observe(captured, raw, capture_work);
        result.captured = captured.complete();
        // Per-frame cell digests make the fifteen captures auditable in the
        // full report while the compact proof carries the closed booleans.
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
            need(found.has_value(), "source: ERGroupEdge proposal case binding");
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
                 "physical: ERGroupEdge cache modes share all five planes");
        const bool restored = (f[0] == f[2]);
        const bool wide_source = (f[0] == f[3]);
        const bool narrow_source = (f[1] == f[4]);
        const bool a = rel::following_row(f[0], geometry_work, kSentinel).has_value();
        const bool b = rel::following_row(f[1], geometry_work, kSentinel).has_value();
        const bool c = rel::following_row(f[2], geometry_work, kSentinel).has_value();
        const auto geo_errors =
            rel::validate_impl(f[0], model, geometry_work, false, nullptr, false, nullptr,
                               kSentinel);
        need(geo_errors.empty(),
             geo_errors.empty() ? "" : ("geometry: ERGroupEdge complete frame " + geo_errors.front()));
        const auto boxes = rel::outlines(f[0], geometry_work);
        need(boxes.size() > model.groups.size(),
             "geometry: ERGroupEdge entity and group contours detected");
        int body_right = 0, body_bottom = 0;
        for (const auto& rect : boxes) {
            body_right = std::max(body_right, rect.r);
            body_bottom = std::max(body_bottom, rect.b);
        }
        checked = {f[0].width, f[0].height, body_right + 1, body_bottom + 1,
                   narrow_source && b};
        need(restored && a && b && c && !wide_source,
             "geometry: ERGroupEdge restored/wide/narrow following-row contract");
        phase = "mutations";
        // encoded is the canonical typed-model byte extent (the matrix passes
        // pin.model_bytes through the same prepaid storage bound, capped at
        // 1896 by er206_negative_bound_domain); it is never a Grid pixel size.
        const auto model_bytes =
            rel::class481_canonical(case_value.get("whole_scene_model"), binding_work).size();
        const auto changes = rel::er206_mutations(
            f[0], model, model_bytes, mutation_work, kSentinel);
        // orphan_ink is the sole physical ink-placement negative (always one);
        // every other executed control mutates the typed Model and is semantic.
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
        // Identity sets are unique; multiplicity is carried by the two counters.
        std::ranges::sort(executed);
        executed.erase(std::unique(executed.begin(), executed.end()), executed.end());
        std::ranges::sort(skipped);
        skipped.erase(std::unique(skipped.begin(), skipped.end()), skipped.end());
        need(changes.executed.count("relation_label") &&
                 changes.executed.count("start_cardinality") &&
                 changes.executed.count("end_cardinality"),
             "protocol: ERGroupEdge group-edge label/cardinality controls present");
        need(changes.executed.count("group_caption"),
             "protocol: ERGroupEdge group caption control present");
        const bool has_nested =
            std::ranges::any_of(model.groups, [](const rel::Group& g) { return !g.parent.empty(); });
        need(has_nested == static_cast<bool>(changes.executed.count("group_parent")),
             "protocol: ERGroupEdge group_parent control matches nested group tree");
        need(result.semantic > 0, "protocol: ERGroupEdge meaningful semantic negatives");
        phase = "complete";
        result.ready = true;
    } catch (const std::bad_alloc&) {
        result.resource = true;
        error = "resource: ERGroupEdge Official allocation failed";
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
    need(diagnostics.good(), "resource: ERGroupEdge evidence output failed");
    compact.set_root(proof);
    result.proof = compact.to_string();
    return result;
}

inline bool owned(json::JsonVal proof) {
    const auto ordinal = proof.get("source_case_ordinal");
    need(ordinal.is_int() && ordinal.as_int() >= 0 &&
             ordinal.as_int() < static_cast<int>(source::kCohortSize),
         "source: ERGroupEdge owned source ordinal range");
    const auto index = static_cast<std::size_t>(ordinal.as_int());
    json::JsonMutDoc expected;
    auto root = expected.object();
    metadata(root, index);
    expected.set_root(root);
    const auto parsed = json::parse(expected.to_string());
    need(parsed.has_value(), "protocol: ERGroupEdge owned metadata JSON");
    rel::Work pins;
    parsed->root().iter_obj([&](json::JsonVal key, json::JsonVal value) {
        rel::class481_require(
            rel::class481_canonical(proof.get(key.as_str()), pins) ==
                rel::class481_canonical(value, pins),
            "source: ERGroupEdge owned literal proof pin");
    });
    const std::set<std::string> runtime{
        "local_ready", "captures", "captures_before_binding", "bound_after_captures",
        "full_frames_published", "geometry_and_restoration_good", "narrow_exact_ordinary",
        "narrow_geometry_good", "uncached_cached_five_planes_equal", "cache_hit_observed",
        "width", "height", "body_width", "body_height", "semantic_mutations_rejected",
        "physical_mutations_rejected", "verification_resource_exhausted", "phase",
        "mutations_executed", "mutations_inapplicable", "error"};
    proof.iter_obj([&](json::JsonVal key, json::JsonVal) {
        need(parsed->root().has(key.as_str()) || runtime.contains(std::string(key.as_str())),
             "source: ERGroupEdge closed Official case proof");
    });
    const auto yes = [&](std::string_view key) {
        const auto v = proof.get(key);
        need(v.is_bool(), "source: ERGroupEdge Official proof boolean");
        return v.as_bool();
    };
    if (!yes("local_ready")) {
        need(proof.get("error").is_str() && !proof.get_string("error").empty(),
             "source: ERGroupEdge failed proof has explicit error");
        return false;
    }
    need(!proof.has("error") && !yes("verification_resource_exhausted"),
         "source: ERGroupEdge good proof has no error/resource");
    need(proof.get("captures").is_int() && proof.get_int("captures") == 15,
         "source: ERGroupEdge fifteen actual public frames");
    for (const auto key : {"captures_before_binding", "bound_after_captures",
                           "full_frames_published", "geometry_and_restoration_good",
                           "uncached_cached_five_planes_equal", "cache_hit_observed"})
        need(yes(key), "source: ERGroupEdge capture/bind/cache responsibility");
    need(proof.get_string("phase") == "complete",
         "source: ERGroupEdge completed verification phase");
    for (const auto key : {"height", "body_width", "body_height"})
        need(proof.get(key).is_int() && proof.get_int(key) > 0 &&
                 proof.get_int(key) <= 1040,
             "source: ERGroupEdge complete extent bounds");
    // width is the fixed 240-column public capture width for every case; the
    // narrow outcome is measured independently from the 40-column frames and is
    // never inferred from this capture width.
    need(proof.get("width").is_int() && proof.get_int("width") == 240,
         "source: ERGroupEdge fixed 240-column capture width");
    need(yes("narrow_exact_ordinary") != yes("narrow_geometry_good"),
         "source: ERGroupEdge narrow exact-ordinary/full-geometry XOR");
    need(proof.get("semantic_mutations_rejected").is_int() &&
             proof.get_int("semantic_mutations_rejected") > 0 &&
             proof.get("physical_mutations_rejected").is_int() &&
             proof.get_int("physical_mutations_rejected") >= 1,
         "source: ERGroupEdge meaningful semantic controls and physical orphan");
    const auto executed = proof.get("mutations_executed"),
               skipped = proof.get("mutations_inapplicable");
    need(executed.is_arr() && skipped.is_arr(),
         "source: ERGroupEdge mutation identity arrays");
    std::set<std::string> ids;
    for (std::size_t i = 0; i < executed.size(); ++i) {
        const auto id = executed.at(i);
        const std::string name(id.as_str());
        need(id.is_str() && ids.insert(name).second &&
                 executed_vocabulary().contains(name),
             "source: ERGroupEdge unique executed control in closed vocabulary");
    }
    std::set<std::string> skip_ids;
    for (std::size_t i = 0; i < skipped.size(); ++i) {
        const auto id = skipped.at(i);
        const std::string name(id.as_str());
        need(id.is_str() && skip_ids.insert(name).second &&
                 skipped_vocabulary().contains(name),
             "source: ERGroupEdge unique skipped control in closed vocabulary");
    }
    need(ids.contains("relation_label") && ids.contains("start_cardinality") &&
             ids.contains("end_cardinality") && ids.contains("group_caption"),
         "source: ERGroupEdge cohort-defining controls present");
    return true;
}
}  // namespace loom::test::mermaid_er_groupedge::official
