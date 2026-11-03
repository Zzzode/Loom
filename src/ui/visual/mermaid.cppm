module;

#include <ftxui/dom/node.hpp>

export module loom.ui.visual.mermaid;

import std;

export namespace loom::ui::mermaid {

/// Render the admitted Mermaid subset, preserving the caller's source element
/// when a successfully laid out diagram does not fit the assigned width.
[[nodiscard]] std::optional<ftxui::Element> render_mermaid(std::string_view source,
                                                           ftxui::Element fallback);

/// Explicit ASCII option for the admitted GitGraph profile. Other ASCII
/// families retain the caller-owned fallback until their profile is admitted.
[[nodiscard]] std::optional<ftxui::Element> render_mermaid(std::string_view source,
                                                           ftxui::Element fallback, bool use_ascii);

/// RFC 0031 E6b: per-instance LRU cache of immutable laid-out diagram ROWS
/// (std::vector<std::string>), never ftxui::Element. Width/theme are not key
/// components: a fresh node is built per render from the cached rows, so a
/// width toggle stays inside the node and never invalidates entries.
///
/// The cache is owned per instance (one StreamingMarkdown, one message
/// list); it is never process-global and is unrelated to the 256-entry
/// process-global markdown token LRU.
namespace testing {
struct Work;
struct FinalEmissionTrace;
struct CacheBudgetAccess;
}  // namespace testing

class LayoutRowsCache {
public:
    /// @param max_entries  bounded entry count (LRU eviction)
    /// @param max_bytes    bounded sum of retained row bytes (LRU eviction)
    LayoutRowsCache(std::size_t max_entries, std::size_t max_bytes);
    ~LayoutRowsCache();
    // Deep copy: the hash index is rebuilt against the new LRU nodes. Owners
    // (MessagesListInput) are copyable value structs; a copy starts with the
    // same retained rows but has independent LRU state thereafter.
    LayoutRowsCache(const LayoutRowsCache&);
    LayoutRowsCache& operator=(const LayoutRowsCache&);
    LayoutRowsCache(LayoutRowsCache&&) noexcept;
    LayoutRowsCache& operator=(LayoutRowsCache&&) noexcept;

    /// Return the rows for (source, use_ascii), laying out on a miss and
    /// retaining the result. A present EMPTY vector is the cached
    /// whole-source-rejection sentinel (distinct from the returned
    /// std::optional being engaged): the caller must still run the normal
    /// finish/fallback path so the surface is reproduced exactly.
    [[nodiscard]] std::optional<std::vector<std::string>> get_or_layout(std::string_view source,
                                                                        bool use_ascii);

    /// Get-or-layout through this cache, then finish into a FRESH node via
    /// the same finish step render_mermaid uses. Cached rejections reproduce
    /// the whole-source fallback (nullopt). This is the cached drop-in for
    /// the two render_mermaid entry points; no Element is ever retained.
    [[nodiscard]] std::optional<ftxui::Element> render(std::string_view source, bool use_ascii,
                                                       ftxui::Element fallback);

    /// Drop every retained entry (document/session reset boundary).
    void clear();

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t byte_size() const noexcept;
    [[nodiscard]] std::size_t entry_capacity() const noexcept;
    [[nodiscard]] std::size_t byte_capacity() const noexcept;
    [[nodiscard]] std::size_t hits() const noexcept;
    [[nodiscard]] std::size_t misses() const noexcept;

private:
    friend struct testing::CacheBudgetAccess;
    std::optional<ftxui::Element> render_with_work(std::string_view source, bool use_ascii,
                                                   ftxui::Element fallback, testing::Work& work,
                                                   testing::FinalEmissionTrace* trace);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace loom::ui::mermaid

// Sole reviewed test seam (RFC 0031 E6a). These complete types are exported
// so the engine tests can exercise the closed parsers without attaching their
// translation units to the production module. Tests must reach them ONLY
// through the loom.ui.visual.mermaid.testing facade module; production code
// refers to the same entities through non-exported aliases in the
// loom.ui.visual.mermaid:detail implementation partition. Declarations and
// data initializers only; every body lives in an implementation unit.
export namespace loom::ui::mermaid::testing {

inline constexpr std::size_t kWorkLimit = 1'000'000;

// ADR 0032: independent worst-case ceiling for the real ftxui Element-tree
// emission on the active-ER packed path: ElementWork(R,U)=320R+512+416U with
// R<=1024 and U<=240*R. Derived from the 240x1024 canvas contract, never tuned.
inline constexpr std::size_t kErElementWorkLimit = 102'564'352;

struct Work {
    std::size_t remaining = kWorkLimit;
    bool spend(std::size_t count = 1);
};

// Structural metadata envelope shared by every family. The reader extracts
// frontmatter structure and init directives; family-specific value validation
// stays in each family.
// Test-only opt-in for the unadmitted Flow style prototype.
bool set_flow_style_preview_for_testing(bool enabled);

// Passive observation of the actual styled final-Element budget boundary.
struct FinalEmissionTrace {
    bool styled_entered = false, preflight_complete = false, allocation_started = false,
         returned_native = false;
    std::size_t entry_remaining = 0u, emission_debit = 0u, preflight_debits = 0u,
                successful_debits = 0u;
    std::size_t last_debit = 0u, failed_debit = 0u;
    // ADR 0032 ElementWork meter (active-ER packed payloads only). Observation:
    // element_active marks the ER meter path, element_cap is the bound in
    // effect, element_debit accumulates the real Element fees actually charged
    // and element_failed records the first refused fee. Injection: when
    // element_work is non-null on ER entry, that caller-owned meter replaces
    // the fresh capped one, letting tests assert exact-budget (remaining==0)
    // and one-unit-short rejection. Non-ER families leave all four untouched.
    bool element_active = false;
    std::size_t element_cap = 0u, element_debit = 0u, element_failed = 0u;
    Work* element_work = nullptr;
};

// Testing-only budget injection into the exact public render pipeline.
std::optional<ftxui::Element> render_mermaid_with_work(std::string_view source,
                                                       ftxui::Element fallback, bool use_ascii,
                                                       Work& work,
                                                       FinalEmissionTrace* trace = nullptr);

// Budget injection forwards to the exact production cache lookup and finish.
struct CacheBudgetAccess {
    static std::optional<ftxui::Element> render(LayoutRowsCache& cache, std::string_view source,
                                                bool use_ascii, ftxui::Element fallback, Work& work,
                                                FinalEmissionTrace* trace = nullptr);
};

enum class EnvelopeScalarForm : unsigned char { Plain, Quoted, Literal, Folded };

struct EnvelopeScalar {
    std::string text;
    std::string header;  // the raw token after the colon (`>-`, `|`, or the inline value)
    EnvelopeScalarForm form = EnvelopeScalarForm::Plain;
    std::size_t line = 0;
};

struct EnvelopeEvent {
    std::string path;
    std::string value;
    int indent = 0;
    std::size_t line = 0;
    EnvelopeScalarForm form = EnvelopeScalarForm::Plain;
    bool top_level = false;
    bool namespace_entry = false;
    bool comment = false;
    bool blank = false;
};

struct EnvelopeInit {
    std::string raw;
    std::string json;
    std::size_t begin = 0;
    std::size_t end = 0;
};

struct Envelope {
    std::string_view raw_frontmatter;
    std::string_view raw_body;
    std::string_view effective_body;
    std::vector<EnvelopeEvent> events;
    std::vector<EnvelopeEvent> config;
    std::optional<EnvelopeScalar> title;
    std::optional<EnvelopeScalar> description;
    std::vector<EnvelopeInit> init;
    bool has_frontmatter = false;
};

std::optional<Envelope> parse_envelope(std::string_view source, Work& work);

enum class GitDirection { LR, TB, BT };

enum class GitType { Normal, Reverse, Highlight };

struct GitCommit {
    std::string id;
    std::string branch;
    std::string operation;
    GitType type = GitType::Normal;
    std::vector<std::string> tags;
    std::vector<std::string> parents;
    std::size_t sequence = 0u;
};

struct GitBranch {
    std::string name;
    std::string start = "∅";
    std::string head = "∅";
    std::size_t declaration = 0u;
    int order = -1;
};

struct GitGraph {
    GitDirection direction = GitDirection::LR;
    std::string main_branch = "main";
    std::string current_branch = "main";
    std::vector<GitCommit> commits;
    std::vector<GitBranch> branches;
    std::size_t automatic_id = 0u;
    bool commit_after_reset = false;
};

std::optional<GitGraph> parse_gitgraph(std::string_view source, Work& work);

// Canonical canvas rows for the closed ASCII Flow profile, before FTXUI
// Screen padding.
struct FlowAsciiRows {
    std::vector<std::string> native;
    std::vector<std::string> ascii;
};

std::optional<FlowAsciiRows> flow_ascii_rows(std::string_view source, Work& work);

// Reviewed test seam for Class relation-layout resource boundaries.
std::optional<std::vector<std::string>> class_relations_rows(std::string_view source, Work& work,
                                                             bool class_envelope);
std::optional<std::vector<std::size_t>> class_note_candidate_order(std::span<const int> scores,
                                                                   Work& work);

// Internal nonvisual Class metadata inspected through the production parser.
struct ClassAccessibilityMetadata {
    std::optional<std::string> title, description;
};

std::optional<ClassAccessibilityMetadata>
class_accessibility_metadata(std::string_view source, Work& work, bool class_envelope);

// Fixed-corridor probe of actual strict/relaxed routing and hard group-port
// reservation. Results: strict before, relaxed before, reserved, restored, sealed.
std::optional<std::array<bool, 5>> flow_group_border_probe(bool blocked, bool crossable,
                                                           Work& work);

// RFC 0031 E4: ordered family probe classification, factored out of the
// dispatcher. The scan is purely textual (it must not gate on parse_envelope,
// which rejects shapes the textual classifier historically tolerated); the
// execution switch spends the same charges the legacy flag cluster did.
enum class MermaidProbeFamily : unsigned char {
    Flow,
    Sequence,
    GitGraph,
    Class,
    Er,
    State,
    Requirement,
    Charts,
    Mindmap,
    Block
};

struct MermaidProbeDecision {
    MermaidProbeFamily family = MermaidProbeFamily::Charts;
    bool class_envelope = false;
    bool requirement_input = false;
    // RFC 0031 E5: the former per-family prepay accumulator is folded into
    // the single classify_mermaid probe fee; this stays zero for the switch.
    std::size_t prepay = 0;
};

std::optional<MermaidProbeDecision> classify_mermaid(std::string_view source, Work& work);
// Test-only live-budget probe; it accepts source, never a fabricated payload.
bool block_layout_succeeds(std::string_view source, Work& work);
// Exact public-factory path, including dispatch and payload validation.
bool layout_succeeds(std::string_view source, bool use_ascii, Work& work);

}  // namespace loom::ui::mermaid::testing
