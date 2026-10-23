module;

#include <ftxui/dom/node.hpp>

export module loom.ui.visual.mermaid;

import std;

export namespace loom::ui::mermaid {

/// Render the admitted Mermaid subset, preserving the caller's source element
/// when a successfully laid out diagram does not fit the assigned width.
[[nodiscard]] std::optional<ftxui::Element> render_mermaid(
    std::string_view source, ftxui::Element fallback);

/// Explicit ASCII option for the admitted GitGraph profile. Other ASCII
/// families retain the caller-owned fallback until their profile is admitted.
[[nodiscard]] std::optional<ftxui::Element> render_mermaid(
    std::string_view source, ftxui::Element fallback, bool use_ascii);

} // namespace loom::ui::mermaid

// Private cell primitives shared only by this module's implementation units.
namespace loom::ui::mermaid::detail {

constexpr std::size_t kSourceLimit = 65536;
constexpr int kWidthLimit = 240;
constexpr int kHeightLimit = 1024;
constexpr int kLabelLimit = 256;
constexpr std::size_t kWorkLimit = 1000000;
constexpr std::size_t kGroupLimit = 64;
constexpr std::size_t kGroupDepthLimit = 16;

bool unicode_text(std::string_view text);
std::optional<std::string> validated_plain_label(std::string_view text);

// Structural metadata envelope shared by every family. The reader extracts
// frontmatter structure and init directives; family-specific value validation
// stays in each family.
struct Work;

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

struct Work {
    std::size_t remaining = kWorkLimit;
    bool spend(std::size_t count = 1);
};
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

std::optional<std::vector<std::string>> sequence_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> charts_layout(std::string_view source, Work& work);
std::optional<std::vector<std::string>> gitgraph_layout(std::string_view source, Work& work, bool use_ascii = false);
std::optional<std::vector<std::string>> relations_layout(std::string_view source, Work& work, bool class_envelope = false, bool requirement_input = false);

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

// Same-module test seam for the closed ASCII Flow profile. These rows are
// canonical canvas rows before FTXUI Screen padding; they are not exported.
struct FlowAsciiRows {
    std::vector<std::string> native;
    std::vector<std::string> ascii;
};
std::optional<FlowAsciiRows> flow_ascii_rows(std::string_view source, Work& work);

} // namespace loom::ui::mermaid::detail
