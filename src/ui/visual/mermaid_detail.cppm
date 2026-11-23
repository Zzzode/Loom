// Non-exported module implementation partition (RFC 0031 E6a): the real
// module-privacy boundary. Entities declared here are visible to the module's
// own implementation units via `import :detail;` but cannot be named by any
// external translation unit, and the partition itself is not importable
// outside this module (partitions are never reachable cross-module except
// through the primary's export-import).
//
// The reviewed test seam entities (Work, the Envelope/GitGraph/FlowAsciiRows/
// probe clusters and their parser entry points) are DEFINED in the exported
// loom::ui::mermaid::testing namespace of the primary interface; an exported
// function signature cannot name a partition-only type, so the dependency
// points this way and not in reverse. Production code refers to the seam
// entities through the non-exported aliases below.
module;

#include <ftxui/dom/node.hpp>

module loom.ui.visual.mermaid:detail;

import std;
// Partitions do not implicitly see the primary interface unit (unlike plain
// implementation units); this import is mandatory.
import loom.ui.visual.mermaid;

namespace loom::ui::mermaid::detail {

// Non-exported aliases of every seam-homed entity. The detail parser entry
// points (parse_envelope / parse_gitgraph / flow_ascii_rows /
// classify_mermaid) are NOT aliased: they are detail functions taking the
// aliased seam types, and the testing wrappers forward to them.
using loom::ui::mermaid::testing::Envelope;
using loom::ui::mermaid::testing::EnvelopeEvent;
using loom::ui::mermaid::testing::EnvelopeInit;
using loom::ui::mermaid::testing::EnvelopeScalar;
using loom::ui::mermaid::testing::EnvelopeScalarForm;
using loom::ui::mermaid::testing::ErSourceProvenance;
using loom::ui::mermaid::testing::FinalEmissionTrace;
using loom::ui::mermaid::testing::FlowAsciiRows;
using loom::ui::mermaid::testing::GitBranch;
using loom::ui::mermaid::testing::GitCommit;
using loom::ui::mermaid::testing::GitDirection;
using loom::ui::mermaid::testing::GitGraph;
using loom::ui::mermaid::testing::GitType;
using loom::ui::mermaid::testing::kWorkLimit;
using loom::ui::mermaid::testing::kErElementWorkLimit;
using loom::ui::mermaid::testing::kErEnvelopeLayoutAllowance;
using loom::ui::mermaid::testing::kBlockElementWorkLimit;
using loom::ui::mermaid::testing::kBlockPayloadWorkLimit;
using loom::ui::mermaid::testing::MermaidProbeDecision;
using loom::ui::mermaid::testing::MermaidProbeFamily;
using loom::ui::mermaid::testing::Work;

// Detail implementations of the seam entry points. The signatures name the
// aliased seam types, so they are distinct functions from the testing::
// forwarders (which call these); production code calls these directly.
std::optional<Envelope> parse_envelope(std::string_view source, Work& work);
std::optional<GitGraph> parse_gitgraph(std::string_view source, Work& work,
                                       std::string_view main_branch = "main",
                                       int main_branch_order = -1);
std::optional<FlowAsciiRows> flow_ascii_rows(std::string_view source, Work& work);
std::optional<MermaidProbeDecision> classify_mermaid(std::string_view source, Work& work);

constexpr std::size_t kSourceLimit = 65536;
constexpr int kWidthLimit = 240;
constexpr int kHeightLimit = 1024;
constexpr int kLabelLimit = 256;
constexpr std::size_t kGroupLimit = 64;
constexpr std::size_t kGroupDepthLimit = 16;
// RFC 0048: concurrent regions per State composite. The bound keeps the
// side-by-side composition (one divider column per boundary) inside the
// existing width limit for the smallest admissible region.
constexpr int kStateRegionLimit = 8;

bool unicode_text(std::string_view text);
std::optional<std::string> validated_plain_label(std::string_view text);

// D1a payloads own semantic roles and text, never theme colors or Elements.
enum class SemanticColorRole : unsigned char {
    Inherit,
    Neutral,
    Red,
    Orange,
    Yellow,
    Green,
    Cyan,
    Blue,
    Purple
};

struct SemanticCellStyle {
    SemanticColorRole foreground = SemanticColorRole::Inherit, fill = SemanticColorRole::Inherit;
    bool bold = false, underline = false;
    bool operator==(const SemanticCellStyle&) const = default;
};

struct StyledRun {
    std::string text;
    SemanticCellStyle style;
};

struct StyledRows {
    std::vector<std::vector<StyledRun>> rows;
};

// Which packed family, if any, owns an independent Work meter separate from
// the layout pool. ER meters only the real Element fees (ADR 0032); Block
// meters the whole post-block_layout payload preparation — gates, finish
// bookkeeping and Element fees (D-035 batch 2). Off for every other family.
// payload_prepaid is the amount ErPaint prepaid for the same payload's
// gates/bookkeeping (ER only; Block has no producer prepayment).
enum class PackedMeter : unsigned char { Off, Er, Block };
struct NativeLayout {
    std::vector<std::string> rows;
    std::optional<StyledRows> styles;
    std::size_t measured_layout_work = 0u;
    std::vector<std::uint16_t> packed_styles{};
    PackedMeter packed_meter = PackedMeter::Off;
    std::size_t payload_prepaid = 0u;
    // Block payload-preparation meter (D-035 batch 2): live after the gates
    // on a miss; reconstructed on a cache hit with measured_payload_work
    // re-debited. Unused for ER and non-packed families.
    Work payload_work{};
    std::size_t measured_payload_work = 0u;
};

// RFC 0031 FS-3a graduated classDef/class/direct-style projection; only the
// handDrawn look metadata profile (FS-8) remains a test-facade-only preview.
bool set_flow_handdrawn_preview_for_testing(bool enabled);
bool flow_handdrawn_preview_active();
std::optional<NativeLayout> layout_mermaid_content(std::string_view source, bool use_ascii,
                                                   Work& work);
std::optional<ftxui::Element> render_mermaid_with_budget(std::string_view source,
                                                         ftxui::Element fallback, bool use_ascii,
                                                         Work& work,
                                                         FinalEmissionTrace* trace = nullptr);
std::optional<ftxui::Element> finish_native_layout(std::optional<NativeLayout> layout,
                                                   ftxui::Element fallback, Work& work,
                                                   FinalEmissionTrace* trace = nullptr);
std::optional<std::size_t> styled_payload_bytes(const NativeLayout& layout,
                                                std::size_t source_bytes);
bool prepay_styled_payload(NativeLayout& layout, std::size_t source_bytes, Work& work,
                           std::size_t* prepaid = nullptr, Work* prep_sink = nullptr);

std::optional<std::vector<std::string>> mindmap_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> journey_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> timeline_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> gantt_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> treemap_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> c4_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> venn_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> quadrant_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> architecture_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> kanban_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> usecase_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> treeview_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> ishikawa_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> cynefin_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> wardley_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> railroad_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> radar_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> packet_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> eventmodeling_layout(std::string_view source,
                                                             Work& work);
std::optional<std::vector<std::string>> agentflow_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> sankey_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> sequence_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> charts_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> gitgraph_layout(std::string_view source, Work& work,
                                                        bool use_ascii = false);
std::optional<std::vector<std::string>>
relations_layout(std::string_view source, Work& work, bool class_envelope = false,
                 bool requirement_input = false, std::optional<StyledRows>* styles = nullptr,
                 std::vector<std::uint16_t>* packed_styles = nullptr,
                 std::size_t* payload_prepaid = nullptr);

// RFC 0031 E6b: the layout-only seam behind both public render_mermaid
// entry points. It runs the current family dispatch with a fresh Work budget
// and returns immutable string ROWS — never an ftxui::Element. Nullopt (or
// an empty vector) is the whole-source rejection outcome; width/theme flips
// happen later inside a freshly constructed node. The per-instance
// LayoutRowsCache calls this seam on a miss.
std::optional<std::vector<std::string>> layout_mermaid_rows(std::string_view source,
                                                            bool use_ascii);

// RFC 0030 Block: one immutable private payload serves both public paths.
// Legacy families keep an empty plane and their exact inherited-style finish.
struct LayoutPayload {
    std::vector<std::string> rows;
    std::vector<std::uint16_t> styles;
};

std::optional<LayoutPayload> block_layout(std::string_view source, Work& work);
// Combined layout-Work credit ErPaint::finish prepaid for the generic packed
// gates and finish bookkeeping on one packed payload (active ER only).
std::size_t packed_payload_prepaid(const std::vector<std::string>& rows,
                                   const std::vector<std::uint16_t>& styles,
                                   std::size_t source_bytes);
bool valid_packed_rows(const std::vector<std::string>& rows,
                       const std::vector<std::uint16_t>& styles, Work& work,
                       std::size_t* prepaid = nullptr, Work* prep_sink = nullptr);
std::optional<ftxui::Element> finish_mermaid_payload(std::optional<LayoutPayload> payload,
                                                     ftxui::Element fallback, Work& work,
                                                     FinalEmissionTrace* trace = nullptr,
                                                     PackedMeter meter = PackedMeter::Off,
                                                     std::size_t* prepaid = nullptr,
                                                     Work* external_meter = nullptr);

struct Point {
    int x = 0;
    int y = 0;
    bool operator==(const Point&) const = default;
};

Point operator+(Point a, Point b);
Point operator-(Point a, Point b);
constexpr Point kSteps[]{{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
constexpr unsigned kRight = 1, kDown = 2, kLeft = 4, kUp = 8;
unsigned direction_bits(Point delta);
std::string_view line_glyph(unsigned bits);
std::string_view arrow(Point delta);
std::vector<std::string_view> label_rows(std::string_view text);
int label_width(std::string_view text);

struct Cell {
    std::string glyph = " ";
    unsigned bits = 0;
    bool blocked = false;
    unsigned allowed = kRight | kDown | kLeft | kUp;
};

// One styled run of a Markdown flow label; '\n' inside text is a visual row
// break. Empty attribute runs are plain.
struct LabelRun {
    std::string text;
    bool bold = false;
    bool underline = false;
    bool operator==(const LabelRun&) const = default;
};

struct Canvas {
    int width;
    int height;
    Work& work;
    std::vector<Cell> cells;
    std::unique_ptr<std::vector<SemanticCellStyle>> styles;
    Canvas(int w, int h, Work& budget);
    bool inside(Point p) const;
    int index(Point p) const;
    Cell& at(Point p);
    const Cell& at(Point p) const;
    bool free(Point p) const;
    bool put(Point p, std::string_view glyph, bool block = true);
    bool text(Point p, std::string_view value);
    bool multiline(Point p, std::string_view value, int width);
    bool segment(Point from, Point to, bool dashed = false);
    // Markdown-label painting: allocate the parallel style plane on demand,
    // merge bold/underline into the cell style, and center/row-break a run
    // list exactly like multiline().
    bool ensure_styles();
    bool mark_style(Point p, const SemanticCellStyle& style);
    bool text_styled(Point p, std::string_view value, const SemanticCellStyle& style);
    bool multiline_rich(Point p, const std::vector<LabelRun>& runs, int width);
    std::vector<std::string> lines();
};

// Relations (class/ER/state/requirement) private surface shared by the
// relations implementation units (RFC 0031 E3). Declarations only: every
// body lives in mermaid_relations_{core,class,er,state,requirement}_impl.cpp.
enum class RelationFamily { Class, Er, State, Requirement };
enum class EntityShape { Box, Rounded, Choice, Start, Final, Fork, Join, Group, Note };
enum class Marker {
    None,
    Arrow,
    Triangle,
    FilledDiamond,
    OpenDiamond,
    One,
    ZeroOne,
    ZeroMany,
    OneMany,
    RequirementCirclePlus,
    RequirementOpenArrow
};

struct Member {
    std::string text;
    bool method = false;
};

struct ErAttribute {
    std::string type, name;
    std::vector<std::string> keys;
    std::string comment;
};

struct RelationEntity {
    std::string id, label, annotation;
    EntityShape shape = EntityShape::Box;
    int owner = -1;
    std::vector<Member> members;
    bool declared = false;
    std::vector<ErAttribute> er_attributes = {};
    bool er_alias = false;
};

struct ClassEndpointText {
    std::string text;
    int w = 0, h = 0;
};

struct ClassEndpointTexts {
    std::optional<ClassEndpointText> source, target;
};
enum class RelationshipKind : unsigned char { Business, ClassNoteLeader };

struct Relationship {
    int from = -1, to = -1;
    Marker start = Marker::None, end = Marker::Arrow;
    bool dotted = false;
    std::string label;
    std::unique_ptr<ClassEndpointTexts> endpoint_texts;
    RelationshipKind kind = RelationshipKind::Business;
};

struct RelationNote {
    int target = -1;
    bool right = true;
    std::string text;
};

// Two source-owned bytes avoid per-entry string allocation. The vector is
// immutable before any effective-direction view is retained by layout/search.
struct StateScopeDirection {
    int owner = -1;
    std::array<char, 2> value{};
};

struct StateTextRecord {
    int entity = -1;
    bool first_equals_default = false;
    std::vector<std::string> body;
};

struct SourceStyleColor {
    std::array<unsigned char, 3> rgb{};
    SemanticColorRole role = SemanticColorRole::Neutral;
    bool transparent = false;
    bool operator==(const SourceStyleColor&) const = default;
};

struct ClassStyleProperties {
    std::optional<SourceStyleColor> fill, stroke, color;
    std::optional<double> width;
    // Requirement D204: CSS font-weight, normalized to a terminal bold cell
    // attribute. Append-only field: the Class/ER/State style grammar never
    // sets it, so their painted cells and legend rows stay byte-identical.
    std::optional<bool> bold;
    bool operator==(const ClassStyleProperties&) const = default;
};

struct ClassDirectStyle {
    std::string target, verbatim, class_name;
    std::size_t source_offset = 0u;
    ClassStyleProperties properties;
    int entity = -1;
};

struct ClassStyleAssignment {
    std::string target, class_name, verbatim;
    std::size_t source_offset = 0u;
};

struct ClassDirectStyles {
    std::vector<ClassDirectStyle> declarations;
    std::vector<ClassStyleAssignment> assignments;
    std::vector<ClassStyleProperties> effective;
    std::vector<std::string> legend;
};

// D228: state-owned direct-style ledger. State nodes are Rounded (including
// nested composite children), so the state path parses and resolves its own
// classDef/class/style statements instead of the Box/top-level Class seam.
struct StateStyleDefinition {
    std::string name;
    std::string verbatim;
    ClassStyleProperties properties;
};
struct StateStyleAssignment {
    std::vector<std::string> targets;
    std::string class_name;
    std::string verbatim;
};
struct StateStyleDirect {
    std::vector<std::string> targets;
    std::string verbatim;
    ClassStyleProperties properties;
};

struct RouteCandidate {
    int estimate = 0, cost = 0, index = 0;

    bool operator>(const RouteCandidate& other) const {
        return std::tie(estimate, cost, index) > std::tie(other.estimate, other.cost, other.index);
    }
};

struct RouteFrontier
    : std::priority_queue<RouteCandidate, std::vector<RouteCandidate>, std::greater<>> {
    using Base = std::priority_queue<RouteCandidate, std::vector<RouteCandidate>, std::greater<>>;
    using Base::Base;

    std::vector<RouteCandidate>& backing() { return this->c; }
};

struct ErPresentation {
    std::string theme = "redux-color", look = "neo";
    std::vector<std::string> config_notices;
    std::vector<std::pair<std::size_t, std::string>> blank_roles;
    // Render-only route order for a successfully committed relaxed rank plan.
    bool ranked_topology = false;
    // Render-only A* capacity; never serialized or included in source/model
    // projections or hashes. RelationLayout exposes this through a const
    // diagram reference, so the scratch backing is explicitly mutable.
    mutable std::vector<RouteCandidate> frontier_backing;
};
struct ErPresentation;

struct RelationDiagram {
    RelationFamily family = RelationFamily::Class;
    std::string direction = "TB";
    std::vector<RelationEntity> entities;
    std::vector<Relationship> edges;
    std::vector<RelationNote> notes;
    std::unique_ptr<ErPresentation> er_presentation;
    std::optional<std::string> diagram_title;
    std::optional<std::string> class_accessibility_title, class_accessibility_description;
    std::optional<bool> class_hide_empty_members_box;
    bool class_namespaces = false;
    bool class_notes = false;
    std::vector<StateScopeDirection> state_scope_directions;
    std::vector<int> class_typed_entities;
    std::vector<StateTextRecord> state_texts;
    // RFC 0031 E2b: accepted non-terminal frontmatter leaves on state
    // sources, projected to the user as a bounded disclosure footer.
    std::vector<std::string> state_config_notices;
    // D-066 F0: the same disclosure channel for accepted non-terminal
    // requirement frontmatter leaves (htmlLabels, logLevel 0-2, fontFamily).
    std::vector<std::string> requirement_config_notices;
    // RFC 0048: State concurrent-region membership. Both tables stay empty
    // unless the source actually uses a `--` divider, so every region-free
    // diagram keeps its byte-identical model and Work charge sequence.
    // `state_region_by_entity` is indexed by entity slot; the count table is
    // read only for composite owners.
    std::vector<int> state_region_by_entity;
    std::vector<int> state_region_count_by_entity;
    std::unique_ptr<ClassDirectStyles> direct_styles;
    int state_region(int index) const;
    int state_region_count(int owner) const;
    std::optional<std::string_view> state_direction(int owner, Work& work) const;
    std::optional<std::size_t> state_text_index(int target, Work& work) const;
    bool state_fragment(int target, std::string&& text, Work& work);
    int find(std::string_view id) const;
    int class_find(std::string_view id, bool group, Work& work) const;
    std::optional<bool> class_typed(int target, Work& work) const;
    int class_entity(const std::string& id, int owner, Work& work, bool declaration = false,
                     std::optional<std::string_view> type = {});
    int class_namespace(std::string_view id, std::string_view label, int owner, Work& work,
                        bool declared);
    int control(EntityShape shape, int owner, Work& work, int region = 0);
    int entity(std::string id, int owner, Work* er_work = nullptr, int region = 0);
};

struct ErTableGeometry {
    std::vector<int> roles, widths, heights;
    int width = 0, height = 0;
};

struct ErMetaValue {
    enum class Kind { Text, Number, Boolean, Other } kind = Kind::Other;
    std::string_view text;
    double number = 0;
    bool boolean = false;
};

struct ErEnvelope {
    std::string_view body;
    std::optional<std::string> title;
    std::unique_ptr<ErPresentation> presentation;
};

struct ClassEnvelope {
    std::string_view body;
    std::optional<std::string> title;
    std::optional<bool> hide_empty_members_box;
    std::optional<bool> hierarchical_namespaces;
};

struct ClassOperand {
    std::string id;
    std::optional<std::string_view> type;
};

struct StateNoteBlock {
    std::string text;
    std::size_t after = 0;
};

// Requirement typed language: the parser/projector lives in its own impl
// unit, but its parsed model crosses into the core entry point, so the
// complete (non-exported) types live here.
enum class RequirementKind : unsigned char {
    Requirement,
    FunctionalRequirement,
    InterfaceRequirement,
    PerformanceRequirement,
    PhysicalRequirement,
    DesignConstraint,
};
enum class RequirementRelation : unsigned char {
    Contains,
    Copies,
    Derives,
    Satisfies,
    Verifies,
    Refines,
    Traces,
};
enum class Risk : unsigned char { Low, Medium, High };
enum class Verification : unsigned char { Analysis, Demonstration, Inspection, Test };
enum class RequirementRole : unsigned char { Requirement, Element };

struct NodeRef {
    RequirementRole role;
    std::size_t index = 0;
    bool operator==(const NodeRef&) const = default;
};

struct RequirementFields {
    RequirementKind kind;
    std::string logical_name;
    std::optional<std::string> id, text;
    std::optional<Risk> risk;
    std::optional<Verification> verification;
};

struct ElementFields {
    std::string logical_name;
    std::optional<std::string> type, doc_ref;
};

using RequirementNode = std::variant<RequirementFields, ElementFields>;

struct RequirementEdge {
    RequirementRelation kind;
    NodeRef source, target;
    std::size_t statement_order = 0;
};

struct RequirementEnvelope {
    std::string_view body;
    // Both physical and decoded forms are kept through typed parse.  They are
    // inert metadata, never a title or text substituted into a box.
    std::string raw_description, decoded_description;
    // D-066 F0: accepted non-terminal config leaves, projected as a bounded
    // disclosure footer. Empty for config-free sources (byte-identical path).
    std::vector<std::string> config_notices;
    // RFC 0037 P3: a projected top-level title. Empty keeps the historical
    // title-free frame byte-identical.
    std::optional<std::string> title;
};

// D204 requirement style statements. They are collected by the requirement
// parser with the same strictness as the Class style grammar, then resolved
// against the typed node ledger and projected onto the shared
// ClassDirectStyles render seam inside project_requirement.
struct RequirementStyleDefinition {
    std::string name;  // classDef name; "default" is the implicit base class
    std::string verbatim;  // logical source line, disclosed in the legend
    ClassStyleProperties properties;
};

struct RequirementStyleAssignment {
    std::string target;  // node logical name
    std::vector<std::string> classes;  // classDef names, in application order
};

struct RequirementStyleDirect {
    std::vector<std::string> targets;  // comma-separated node logical names
    ClassStyleProperties properties;
    std::string verbatim;
};

struct RequirementDiagram {
    RequirementEnvelope envelope;
    std::string direction = "TB";
    std::vector<RequirementNode> nodes;
    std::vector<RequirementEdge> edges;
    std::vector<RequirementStyleDefinition> style_definitions;
    std::vector<RequirementStyleAssignment> style_assignments;
    std::vector<RequirementStyleDirect> style_directs;
};

struct EndpointPort {
    Point border, normal, route;
    Marker marker = Marker::None;
};

struct RelationBox {
    int x = 0, y = 0, w = 0, h = 0;
    int right() const;
    int bottom() const;
};

bool source_style_properties(std::string_view raw, ClassStyleProperties& properties, Work& work);
std::string_view source_style_role_name(SemanticColorRole role);
// D204 shared color seam: hex-only CSS color parsing and the canonical RGB to
// palette-role classification, extracted from the Class style TU so the
// Requirement grammar can normalize CSS named colors through the exact same
// hue boundaries. No UI RGB contract is added: callers still receive roles.
std::optional<SourceStyleColor> source_style_color(std::string_view raw);
SemanticColorRole source_style_role(int red, int green, int blue);
bool class_direct_style(std::string_view line, std::string_view source, RelationDiagram& diagram,
                        Work& work);
bool class_style_assignment(std::string_view statement, std::string_view source,
                            std::string_view target, std::string_view name,
                            RelationDiagram& diagram, Work& work);
bool class_css_assignment(std::string_view line, std::string_view source, RelationDiagram& diagram,
                          Work& work);
bool resolve_class_direct_styles(RelationDiagram& diagram, Work& work,
                                 std::span<const std::string_view> validated_definitions,
                                 std::string_view source);
bool paint_class_direct_styles(Canvas& canvas, const RelationDiagram& diagram,
                               std::span<const RelationBox> boxes);
std::optional<StyledRows> extract_styled_rows(const Canvas& canvas,
                                              std::span<const std::string> lines,
                                              std::size_t leading_rows, std::size_t drawing_rows,
                                              Work& work);

struct ReciprocalEdgePlan {
    int face = 0, from_row = 0, to_row = 0, inset = 0;
};

struct ReciprocalPlan {
    std::vector<ReciprocalEdgePlan> edges;
    int left_spine = 0, right_spine = 0;
};

struct LabelPlacement {
    Point left, right;
    int x = 0, y = 0, w = 0, h = 0;
};

struct LabelCursor {
    int radius = 0, dy = 0;
    bool negative = false;
};

// Core-provided primitives. Default arguments are declared here only.
std::string_view trim_relation(std::string_view value);
std::optional<std::string> relation_literal_label(std::string_view value);
std::optional<std::string> relation_label(std::string_view value);
std::optional<std::string> identifier(std::string_view& rest, bool quoted = false);
std::optional<std::string> er_literal_label(std::string_view value, Work& work,
                                            bool comment = false);
bool er_ascii_equal(std::string_view left, std::string_view right);
std::size_t er_metadata_space(std::string_view text);
ErMetaValue er_yaml_value(std::string_view value, Work& work);
bool er_config_text(std::string_view text, bool stylesheet, Work& work);
bool er_description_text(std::string_view text, Work& work);
std::string_view er_strip_comment(std::string_view value);
std::array<std::string, 4> er_attribute_fields(const ErAttribute& attribute);
bool er_attribute_budget(const ErAttribute& attribute, Work& work);
std::optional<ErTableGeometry> er_table_geometry(const RelationEntity& entity, Work& work);
bool append_relationship(std::vector<Relationship>& edges, Relationship&& edge, Work& work);
int marker_size(Marker marker);

struct RelationLayout {
    const RelationDiagram& diagram;
    Work& work;
    std::vector<RelationBox> boxes;
    std::vector<std::vector<int>> children;
    std::vector<std::vector<int>> scope_edges;
    // Per-scope strict reach order. The Class reciprocal planner reads the root
    // row after layout; region subsets keep their own local order instead.
    std::vector<std::vector<int>> ordered;
    // RFC 0048: container-local divider columns and the local first body row
    // for each composite that laid out concurrent regions (index = owner + 1).
    std::vector<std::vector<int>> region_dividers;
    std::vector<int> region_top;
    std::vector<int> degrees;
    bool class_endpoint_active = false;
    bool compound_scope = false;
    std::unique_ptr<std::array<int, 64>> class_band_rows;
    explicit RelationLayout(const RelationDiagram& d, Work& budget);
    int common_scope(int from, int to);
    int scope_child(int endpoint, int owner);
    bool prepare();
    std::optional<bool> compound_class_columns(const std::vector<std::vector<bool>>& root_reach);
    bool measure(int owner, std::size_t depth);
    bool measure_regions(int owner, std::size_t depth);
    // The shared single-column scope body. `region` selects the region mode:
    // children are placed in the region's own frame and the computed extents
    // are returned instead of committing the owner's box or the canvas.
    bool measure_scope(int owner, std::size_t depth, const std::vector<int>& list,
                       const std::vector<int>& scope_edge_list, std::vector<int>& order,
                       bool region, int* region_width, int* region_height);
    void translate(int index, int x, int y);
    int canvas_width = 0, canvas_height = 0;
    bool ranked_peers = false;
    bool class_reciprocal = false;
};

struct CompoundScopeRoute {
    const RelationLayout& layout;
    Work& work;
    std::array<unsigned char, 64 + kGroupLimit> scopes{};
    std::array<int, 64 + kGroupLimit> header_bottom{};
    std::array<std::size_t, kGroupLimit> transit_groups{};
    std::size_t transit_count = 0;
    CompoundScopeRoute(const RelationLayout& l, Work& w);
    bool prepare(const Relationship& edge, int owner);
    bool inside(Point p) const;
    unsigned structural(Point p) const;
    bool step(Point from, Point to) const;
};

struct RouteSearch {
    Canvas& canvas;
    const RelationLayout& layout;
    int owner;
    Work& work;
    std::vector<int> distance, parent;
    std::vector<unsigned> stamp;
    unsigned generation = 0;
    std::string_view state_scope_direction;
    int self_face_normal = 0, self_face_border = 0;
    const CompoundScopeRoute* compound_route = nullptr;
    const std::vector<Point>* compound_reservations = nullptr;
    int reciprocal_face = 0, reciprocal_spine = 0, reciprocal_inset = 0;
    Point reciprocal_start{}, reciprocal_goal{};
    RouteSearch(Canvas& c, const RelationLayout& l, int scope);
    bool inside(Point p) const;
    bool free(Point p) const;
    bool step(Point from, Point to) const;
    std::optional<std::vector<Point>> path(Point start, Point goal);
};

bool draw_box(Canvas& canvas, const RelationBox& box, bool rounded);
// RFC 0048: `dividers` are container-local columns whose border junctions on
// this box are not routing ports.
std::vector<EndpointPort> ports_for(const RelationEntity& entity, const RelationBox& box,
                                    Marker marker, const RouteSearch& search,
                                    const std::vector<int>& dividers = {});
bool paint_path(Canvas& canvas, std::span<const Point> path, Point before, Point after, bool dotted,
                const CompoundScopeRoute* compound_route = nullptr);
bool paint_port(Canvas& canvas, const EndpointPort& port, bool attach_er = false);
std::optional<LabelPlacement> label_place(Canvas& canvas, const RouteSearch& search,
                                          const Relationship& edge, const EndpointPort& from,
                                          const EndpointPort& to, std::span<const int> regions,
                                          int region, LabelCursor& cursor,
                                          const std::array<Point, 2>* body_anchors = nullptr);

// Family-provided entry functions.
std::optional<ErEnvelope> er_source_envelope(std::string_view source, Work& work);
std::optional<std::string> er_projection_wrap(std::string_view text, Work& work);
std::optional<std::vector<std::string>> er_projection_notices(const RelationDiagram& diagram,
                                                              Work& work);
bool draw_er_table(Canvas& canvas, const RelationEntity& entity, const RelationBox& box);
std::optional<std::string> class_identifier(std::string_view& rest, Work& work,
                                            bool* backtick = nullptr);
std::optional<ClassOperand> class_operand(std::string_view& rest, Work& work,
                                          std::string_view source, bool& source_checked);
std::optional<std::vector<std::string_view>> class_statement_lines(std::string_view source,
                                                                   Work& work);
std::optional<std::string> class_note_text(std::string_view raw, std::string_view original,
                                           bool& source_checked, Work& work);
std::optional<std::string>
class_display_label(std::string_view value, Work& work,
                    const std::string_view* plain_angle_source = nullptr);
bool class_endpoint_text(std::string_view& rest, Relationship& edge, bool source, Work& work);
std::optional<std::string> class_encoded_annotation(std::string_view text, Work& work);
std::optional<Member> class_member_display(std::string_view raw, Work& work);
bool class_inactive_definition(std::string_view line, std::string_view original,
                               std::vector<std::string_view>& definitions, bool& source_checked,
                               Work& work);
std::optional<ClassEnvelope> class_source_envelope(std::string_view source, Work& work);
std::optional<testing::ClassAccessibilityMetadata>
class_accessibility_metadata(std::string_view source, Work& work, bool class_envelope);
const ClassEndpointText* class_caption(const Relationship& edge, bool source);
bool reciprocal_plan(const RelationDiagram& diagram, const RelationLayout& layout, int owner,
                     Work& work, std::optional<ReciprocalPlan>& result);
bool connect_class_caption_pair(Canvas& canvas, RouteSearch& search, const Relationship& edge,
                                const EndpointPort& p, const EndpointPort& q,
                                const ReciprocalEdgePlan* planned, int spine);
bool connect_reciprocal_relation(Canvas& canvas, RouteSearch& search,
                                 const RelationDiagram& diagram, const RelationLayout& layout,
                                 const Relationship& edge, const ReciprocalPlan& plan,
                                 std::size_t edge_index);
bool state_source_lines(std::string_view source, bool& checked, Work& work);
std::optional<std::string> state_description_label(std::string_view raw, Work& work);
std::optional<StateNoteBlock> state_note_block(std::string_view original, std::size_t header,
                                               Work& work);
// D223: state-local transition-label decoder for the finite corpus escapes
// (`\n` -> line break, `#colon;` -> literal colon); the result re-enters the
// shared closed label validator, so no other escape is admitted. Pure like
// relation_label (no Work fee) to keep the labeled-state schedule identical.
std::optional<std::string> state_transition_label(std::string_view raw);
// D228: bounded state classDef/class/style statement parsers.
bool state_parse_classdef(std::string_view line, Work& work, StateStyleDefinition& out);
bool state_parse_class_assignment(std::string_view line, Work& work, StateStyleAssignment& out);
bool state_parse_direct_style(std::string_view line, Work& work, StateStyleDirect& out);
// D228: resolve the state style ledger onto diagram.direct_styles (Rounded and
// nested entities admitted); the family-agnostic paint path then renders it.
bool build_state_direct_styles(
    RelationDiagram& diagram, std::span<const StateStyleDefinition> definitions,
    std::span<const StateStyleAssignment> assignments,
    std::span<const StateStyleDirect> directs, Work& work);
std::optional<RequirementDiagram> parse_requirement(std::string_view source, Work& work);
std::optional<RelationDiagram> project_requirement(const RequirementDiagram& parsed, Work& work);

// D248 ER107 proposal emitter seam: production parse->IR serialization of an ER
// original into its canonical whole_scene_model JSON component. Test-only; no
// renderer behavior depends on it. Nullopt for a non-ER or unadmitted source.
std::optional<std::string> er_whole_scene_json(std::string_view source, Work& work);

// D248 structural source witnesses (seven span/tiling kinds) for the ER107
// source proposal, emitted from the raw original without semantic token
// classification; slice SHAs are added by the test oracle. Test-only.
std::optional<std::string> er_structural_witnesses_json(std::string_view source, Work& work);

// D251 semantic source witnesses (the 24 token kinds) for the ER107 source
// proposal, produced by a parallel lexer that leaves production ErCursor
// untouched. Ordered in the proposal's construct-grouped sequence with UTF-8
// byte offsets; slice SHAs are added by the test oracle. Test-only.
std::optional<std::string> er_semantic_witnesses_json(std::string_view source, Work& work);

// D271 ER107 source_model (the 16-field source-level model) regenerated from
// the raw original by a parallel record builder sharing the D251 grammar
// primitives. Reachable only through the testing facade; no production call
// path, public contract or frozen-gate change. The top-level canonical SHA is
// computed by the test oracle, not emitted here. Test-only.
std::optional<std::string> er_source_model_json(std::string_view source, Work& work);

// D275 ER107 source_binding (20 fields) joining raw byte refs and component
// digests with caller-supplied provenance. The seam never reads manifest/git/
// env/fs; absent provenance fields render as JSON null. Test-only.
std::optional<std::string> er_source_binding_json(std::string_view source, Work& work,
                                                  const ErSourceProvenance& provenance);

// D279 ER107 terminal_model (14 fields) regenerated from the raw original by a
// pure parallel producer. Hues/roles/tokens are closed tables, widths are
// character counts and the only coordinate key is a constant null, so no
// production layout/palette return value is consumed. Reachable only through
// the testing facade; no production call path or frozen-gate change. Test-only.
std::optional<std::string> er_terminal_model_json(std::string_view source, Work& work);

// RFC 0034: authoritative ER projection notice rows as JSON, backing the
// er_projection_notices_json_for_testing facade seam. Test-only.
std::optional<std::string> er_projection_notices_json(std::string_view source, Work& work);

}  // namespace loom::ui::mermaid::detail
