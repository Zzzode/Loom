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
using loom::ui::mermaid::testing::FlowAsciiRows;
using loom::ui::mermaid::testing::GitBranch;
using loom::ui::mermaid::testing::GitCommit;
using loom::ui::mermaid::testing::GitDirection;
using loom::ui::mermaid::testing::GitGraph;
using loom::ui::mermaid::testing::GitType;
using loom::ui::mermaid::testing::kWorkLimit;
using loom::ui::mermaid::testing::MermaidProbeDecision;
using loom::ui::mermaid::testing::MermaidProbeFamily;
using loom::ui::mermaid::testing::Work;

// Detail implementations of the seam entry points. The signatures name the
// aliased seam types, so they are distinct functions from the testing::
// forwarders (which call these); production code calls these directly.
std::optional<Envelope> parse_envelope(std::string_view source, Work& work);
std::optional<GitGraph> parse_gitgraph(std::string_view source, Work& work);
std::optional<FlowAsciiRows> flow_ascii_rows(std::string_view source, Work& work);
std::optional<MermaidProbeDecision> classify_mermaid(std::string_view source, Work& work);

constexpr std::size_t kSourceLimit = 65536;
constexpr int kWidthLimit = 240;
constexpr int kHeightLimit = 1024;
constexpr int kLabelLimit = 256;
constexpr std::size_t kGroupLimit = 64;
constexpr std::size_t kGroupDepthLimit = 16;

bool unicode_text(std::string_view text);
std::optional<std::string> validated_plain_label(std::string_view text);

std::optional<std::vector<std::string>> mindmap_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> sequence_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> charts_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> gitgraph_layout(std::string_view source, Work& work,
                                                        bool use_ascii = false);
struct LayoutPayload;
std::optional<LayoutPayload> relations_layout(std::string_view source, Work& work,
                                              bool class_envelope = false,
                                              bool requirement_input = false);

// RFC 0030 Block: one immutable private payload serves both public paths.
// Legacy families keep an empty plane and their exact inherited-style finish.
struct LayoutPayload {
    std::vector<std::string> rows;
    std::vector<std::uint16_t> styles;
};

std::optional<LayoutPayload> block_layout(std::string_view source, Work& work);
std::optional<LayoutPayload> layout_mermaid_payload(std::string_view source, bool use_ascii);
std::optional<LayoutPayload> layout_mermaid_payload(std::string_view source, bool use_ascii,
                                                    Work& work);
bool valid_layout_payload(const LayoutPayload& payload);
std::optional<ftxui::Element> finish_mermaid_payload(std::optional<LayoutPayload> payload,
                                                     ftxui::Element fallback);

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

struct Canvas {
    int width;
    int height;
    Work& work;
    std::vector<Cell> cells;
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
    std::optional<bool> class_hide_empty_members_box;
    bool class_namespaces = false;
    bool class_notes = false;
    std::vector<StateScopeDirection> state_scope_directions;
    std::vector<int> class_typed_entities;
    std::vector<StateTextRecord> state_texts;
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
    int control(EntityShape shape, int owner, Work& work);
    int entity(std::string id, int owner, Work* er_work = nullptr);
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
};

struct RequirementDiagram {
    RequirementEnvelope envelope;
    std::string direction = "TB";
    std::vector<RequirementNode> nodes;
    std::vector<RequirementEdge> edges;
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
    std::vector<std::vector<int>> ordered;
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
std::vector<EndpointPort> ports_for(const RelationEntity& entity, const RelationBox& box,
                                    Marker marker, const RouteSearch& search);
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
std::optional<RequirementDiagram> parse_requirement(std::string_view source, Work& work);
std::optional<RelationDiagram> project_requirement(const RequirementDiagram& parsed, Work& work);

}  // namespace loom::ui::mermaid::detail
