// Test-private caller-ledger pipeline. No phase creates a new Work allowance.
#pragma once
#include "mermaid_class_defined6_oracle.hpp"

namespace loom::test::mermaid_class_defined6 {
inline constexpr std::string_view kContract = "ClassDefinedStyle6WholeScreen/v1";
inline constexpr std::string_view kAuthority = "source_approved_ClassDefinedStyle6";
inline constexpr std::size_t kSceneLimit = 128000000u, kReportReserve = 262144u;
enum class Phase : int {
    Reserved,
    Acquire,
    Capture,
    Bind,
    Wide,
    Restored,
    Narrow,
    Mutate,
    Complete
};

struct Progress {
    Phase phase = Phase::Reserved;
    std::size_t captured = 0u, actual_negatives = 0u, credited_negatives = 0u;
    bool published = false, bound = false, decoded = false, restored_equal = false,
         narrow_source = false, admitted = false;
    bool exception = false, inconclusive = false, resource = false, semantic_failure = false;
    std::array<int, 2> outcomes{{-1, -1}};
    std::array<std::int64_t, 8> remaining{{-1, -1, -1, -1, -1, -1, -1, -1}};
};

inline unsigned failure_flags(const Progress& p) {
    return static_cast<unsigned>(p.resource) | (static_cast<unsigned>(p.inconclusive) << 1u) |
           (static_cast<unsigned>(p.semantic_failure) << 2u);
}

struct Run {
    Progress progress;
    std::array<std::string, 3> status{{"error", "error", "error"}};
    std::array<Screen, 5> frames;
    std::string actual_source_sha, decoded_model_sha, error;
    std::optional<Expected> expected;
};

inline bool exact_footer(const Screen& screen, std::string_view token, Work& work) {
    if (token.empty() || token.size() > 40u) return false;
    std::optional<int> row;
    work.spend(screen.glyphs.cells.size() * 8u + 1024u);
    for (const auto& cell : screen.glyphs.cells) {
        if (cell.size() > 64u) return false;
        work.spend(cell.size() * 8u + 8u);
    }
    for (int y = 0; y < screen.glyphs.height; ++y) {
        const auto text = screen.glyphs.row(y);
        if (text.find(token) != std::string::npos) {
            if (row || trim(text) != token) return false;
            row = y;
        }
    }
    if (!row) return false;
    for (int y = *row + 1; y < screen.glyphs.height; ++y)
        for (int x = 0; x < screen.glyphs.width; ++x) {
            work.spend();
            const auto cell = screen.glyphs.at({x, y});
            if (cell != " " && !cell.empty()) return false;
        }
    return true;
}

inline bool same_source_surface(const Screen& a, const Screen& b, Work& work) {
    if (a.glyphs.width != b.glyphs.width || a.glyphs.height != b.glyphs.height ||
        a.glyphs.cells.size() != b.glyphs.cells.size() ||
        a.attributes.size() != b.attributes.size() || a.attributes.size() != a.glyphs.cells.size())
        return false;
    work.spend(a.glyphs.cells.size() * (2u * sizeof(Attributes) + 32u) + 1024u);
    for (std::size_t n = 0; n < a.glyphs.cells.size(); ++n) {
        work.spend(a.glyphs.cells[n].size() * 4u + b.glyphs.cells[n].size() * 4u + 8u);
        if (a.glyphs.cells[n] != b.glyphs.cells[n] ||
            a.attributes[n].background != b.attributes[n].background ||
            a.attributes[n].flags != b.attributes[n].flags ||
            a.attributes[n].hyperlink != b.attributes[n].hyperlink)
            return false;
    }
    return true;
}

inline bool same_screen(const Screen& a, const Screen& b, Work& work) {
    if (!same_source_surface(a, b, work)) return false;
    work.spend(a.attributes.size() * 8u + 256u);
    for (std::size_t n = 0; n < a.attributes.size(); ++n)
        if (a.attributes[n].foreground != b.attributes[n].foreground) return false;
    return true;
}

inline std::string fenced(std::string_view source, std::string_view language,
                          std::string_view token) {
    return "```" + std::string(language) + "\n" + std::string(source) + "\n```\n\n" +
           std::string(token) + "\n";
}

inline Run run(std::size_t selected, std::size_t ordinal, std::size_t global, JsonVal fixture,
               Work& work, std::string_view token, auto&& publish,
               std::optional<std::size_t> cutoff = {}) {
    // Failure text, proof objects and later bounded collector copies are paid
    // before an optional test cutoff. This is a reservation on this same ledger.
    work.spend(kReportReserve);
    require(selected < kPins.size(), "ClassDefined6 run index");
    const auto& pin = kPins[selected];
    Run result;
    result.error.reserve(256u);
    result.actual_source_sha.reserve(64u);
    result.decoded_model_sha.reserve(64u);
    if (cutoff) work.left = std::min(work.left, *cutoff);
    result.progress.remaining[0] = static_cast<std::int64_t>(work.left);
    const auto fail_result = [&](Result checked) {
        if (checked.outcome == Outcome::Match) return;
        result.progress.resource |= checked.resource;
        result.progress.inconclusive |= checked.outcome == Outcome::Inconclusive;
        result.progress.semantic_failure |= checked.outcome == Outcome::Mismatch;
        throw std::runtime_error("ClassDefined6 whole-screen validation failed");
    };
    try {
        result.progress.phase = Phase::Acquire;
        work.spend(pin.source_bytes * 24u + 32768u);
        const auto docs = load(work);
        const auto path =
            std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/cases" / pin.fixture;
        require(std::filesystem::file_size(path) == pin.source_bytes,
                "ClassDefined6 bounded original file size");
        std::ifstream input(path, std::ios::binary);
        require(static_cast<bool>(input), "ClassDefined6 full source read");
        const std::string source{std::istreambuf_iterator<char>(input), {}};
        require(source.size() <= 65536u, "ClassDefined6 full source bound");
        const auto theme = loom::ui::design::theme::current_theme();
        auto native = loom::ui::render_markdown(fenced(source, "mermaid", token));
        auto ordinary = loom::ui::render_markdown(fenced(source, "text", token));
        native->ComputeRequirement();
        ordinary->ComputeRequirement();
        const int height = std::min(1040, std::max({32, native->requirement().min_y + 8,
                                                    ordinary->requirement().min_y + 8}));
        result.progress.remaining[1] = static_cast<std::int64_t>(work.left);
        result.progress.phase = Phase::Capture;
        result.frames[0] = capture(native, 240, height, work);
        ++result.progress.captured;
        result.frames[1] = capture(ordinary, 240, height, work);
        ++result.progress.captured;
        result.frames[2] = capture(native, 40, height, work);
        ++result.progress.captured;
        result.frames[3] = capture(ordinary, 40, height, work);
        ++result.progress.captured;
        result.frames[4] = capture(native, 240, height, work);
        ++result.progress.captured;
        publish(result.frames, work);
        result.progress.published = true;
        result.progress.remaining[2] = static_cast<std::int64_t>(work.left);
        result.progress.phase = Phase::Bind;
        work.spend(source.size() * 8u + 8192u);
        result.actual_source_sha = hash(source);
        require(source.find(token) == std::string::npos,
                "ClassDefined6 footer collision outside admitted cohort");
        result.expected = loom::test::mermaid_class_defined6::bind(docs, selected, ordinal, global,
                                                                   source, fixture, work);
        result.progress.bound = result.progress.decoded = true;
        result.decoded_model_sha = pin.model_sha;
        result.progress.remaining[3] = static_cast<std::int64_t>(work.left);
        result.progress.phase = Phase::Wide;
        const auto wide = check(result.frames[0], *result.expected, *theme.palette, work, token);
        result.progress.outcomes[0] = static_cast<int>(wide.outcome);
        fail_result(wide);
        result.progress.remaining[4] = static_cast<std::int64_t>(work.left);
        result.progress.phase = Phase::Restored;
        const auto restored =
            check(result.frames[4], *result.expected, *theme.palette, work, token);
        result.progress.outcomes[1] = static_cast<int>(restored.outcome);
        fail_result(restored);
        result.progress.restored_equal = same_screen(result.frames[0], result.frames[4], work);
        require(result.progress.restored_equal, "ClassDefined6 full-cell attribute restoration");
        result.progress.remaining[5] = static_cast<std::int64_t>(work.left);
        result.progress.phase = Phase::Narrow;
        result.progress.narrow_source =
            same_source_surface(result.frames[2], result.frames[3], work) &&
            exact_footer(result.frames[2], token, work) &&
            exact_footer(result.frames[3], token, work);
        require(result.progress.narrow_source,
                "ClassDefined6 caller-language ordinary narrow source");
        result.progress.remaining[6] = static_cast<std::int64_t>(work.left);
        result.progress.phase = Phase::Mutate;
        require(wide.box.has_value(), "ClassDefined6 good physical mutation witness");
        for (std::size_t kind = 0; kind < kPhysicalNegatives; ++kind) {
            auto changed = copy_screen(result.frames[0], work);
            mutate(changed, *wide.box, kind, work);
            const auto rejected = check(changed, *result.expected, *theme.palette, work, token);
            if (rejected.outcome != Outcome::Mismatch) {
                result.progress.resource |= rejected.resource;
                result.progress.inconclusive |= rejected.outcome == Outcome::Inconclusive;
                throw std::runtime_error("ClassDefined6 nondecisive physical adversary");
            }
            ++result.progress.actual_negatives;
        }
        result.progress.remaining[7] = static_cast<std::int64_t>(work.left);
        result.progress.credited_negatives = kPhysicalNegatives;
        result.progress.admitted = true;
        result.progress.phase = Phase::Complete;
        result.status = {"drawn", "source_fallback", "drawn"};
    } catch (const std::exception& error) {
        result.progress.exception = true;
        const char* text = error.what();
        std::size_t size = 0u;
        if (text)
            while (size < 256u && text[size])
                ++size;
        result.progress.resource |= work.left == 0u ||
                                    dynamic_cast<const std::bad_alloc*>(&error) != nullptr ||
                                    resource_message(std::string_view(text ? text : "", size));
        result.progress.inconclusive |= !result.progress.semantic_failure;
        result.error.assign(text ? text : "ClassDefined6 exception", text ? size : 17u);
        result.progress.admitted = false;
        result.progress.credited_negatives = 0u;
        result.status = {"error", "error", "error"};
    }
    return result;
}

inline void publish_frames(loom::utils::json::JsonMutVal item, loom::utils::json::JsonMutDoc& owner,
                           const std::array<Screen, 5>& frames, Work& work) {
    // Budget the whole publication before allocating any partially populated
    // subtree. The caller attaches it only after all actual cells are present.
    work.spend(4096u);
    std::size_t cells = 0u, bytes = 0u;
    for (const auto& frame : frames) {
        require(frame.glyphs.width > 0 && frame.glyphs.width <= 240 && frame.glyphs.height > 0 &&
                    frame.glyphs.height <= 1040 &&
                    frame.glyphs.cells.size() ==
                        static_cast<std::size_t>(frame.glyphs.width * frame.glyphs.height) &&
                    frame.attributes.size() == frame.glyphs.cells.size(),
                "ClassDefined6 capture publication bounds exceeded");
        work.spend(frame.glyphs.cells.size() * 8u + 1024u);
        cells += frame.glyphs.cells.size();
        for (const auto& cell : frame.glyphs.cells) {
            require(cell.size() <= 64u, "ClassDefined6 capture cell bounds exceeded");
            bytes += cell.size();
        }
    }

    work.spend(cells * 512u + bytes * 32u + 65536u);
    auto published = owner.array();
    for (const auto& frame : frames) {
        auto value = owner.object(), pixels = owner.array();
        value.set("width", frame.glyphs.width);
        value.set("height", frame.glyphs.height);
        for (std::size_t n = 0; n < frame.glyphs.cells.size(); ++n) {
            auto cell = owner.array();
            cell.append(owner.string(frame.glyphs.cells[n]));
            cell.append(owner.string(frame.attributes[n].foreground.Print(true)));
            cell.append(owner.string(frame.attributes[n].background.Print(false)));
            cell.append(owner.number(static_cast<std::int64_t>(frame.attributes[n].flags)));
            cell.append(owner.number(static_cast<std::int64_t>(frame.attributes[n].hyperlink)));
            pixels.append(cell);
        }
        value.add("actual_cells", pixels);
        published.append(value);
    }
    item.add("actual_five_frames", published);
}
}  // namespace loom::test::mermaid_class_defined6

namespace loom::test::mermaid_class_defined6 {
inline std::optional<std::size_t> ordinal_index(std::size_t ordinal) {
    for (std::size_t i = 0; i < kPins.size(); ++i)
        if (kPins[i].ordinal == ordinal) return i;
    return {};
}

inline std::string capture_file(std::string_view owner, std::size_t selected) {
    require(selected < kPins.size() && owner.size() <= 32u, "ClassDefined6 capture owner/index");
    return "mermaid-class-defined6-" + std::string(owner) + "-" +
           std::to_string(kPins[selected].ordinal) + "-actual-five-frames.json";
}

inline void publish_file(std::string_view source_owner, std::size_t selected,
                         const std::array<Screen, 5>& frames, Work& work) {
    require(selected < kPins.size(), "ClassDefined6 published source index");
    const auto& pin = kPins[selected];
    loom::utils::json::JsonMutDoc document;
    auto root = document.object();
    root.set("schema", "ClassDefined6ActualCapture/v1");
    root.set("fixture", pin.fixture);
    root.set("expected_source_sha256", pin.source_sha);
    root.set("source_bound_at_publication", false);
    root.set("semantic_Model_decoded_at_publication", false);
    publish_frames(root, document, frames, work);
    document.set_root(root);
    const auto text = document.to_string();
    const auto path =
        std::filesystem::path(LOOM_MERMAID_REPORT_DIR) / capture_file(source_owner, selected);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(static_cast<bool>(output), "ClassDefined6 capture file open");
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    require(output.good(), "ClassDefined6 capture file complete write");
}

inline void invalidate(Run& result, const std::exception& error, Work& work) {
    const char* text = error.what();
    std::size_t size = 0u;
    if (text)
        while (size < 256u && text[size])
            ++size;
    result.progress.exception = true;
    result.progress.resource |= work.left == 0u ||
                                dynamic_cast<const std::bad_alloc*>(&error) != nullptr ||
                                resource_message(std::string_view(text ? text : "", size));
    result.progress.inconclusive = true;
    result.progress.admitted = false;
    result.progress.credited_negatives = 0u;
    result.status = {"error", "error", "error"};
    result.error.assign(text ? text : "ClassDefined6 exception", text ? size : 17u);
}
}  // namespace loom::test::mermaid_class_defined6
