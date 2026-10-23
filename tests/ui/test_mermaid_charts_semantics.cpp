// RFC 0030 chart oracle: release-pinned public inputs and independent terminal
// geometry. No foreign parser, layout implementation, or runtime is executed.
#include <gtest/gtest.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>

import std;
import loom.ui.visual.markdown;
import loom.serdes.json;
import loom.crypto.crypto;

#include "mermaid_chart_cell_oracle.hpp"

namespace {
namespace json = loom::utils::json;
using json::JsonVal;
using namespace loom::test::mermaid_chart;
constexpr std::string_view kSentinel = kDefaultFollowingToken;
constexpr int U = 1,R = 2,D = 4,L = 8;
struct Stroke { int bits = 0,family = 0; };
bool braille_composite(std::string_view text) {
    if (text.size() != 3) return false;
    const auto first = static_cast<unsigned char>(text[0]);
    const auto second = static_cast<unsigned char>(text[1]);
    const auto third = static_cast<unsigned char>(text[2]);
    return first == 0xe2 && second >= 0xa0 && second <= 0xa3 && third >= 0x80 && third <= 0xbf &&
        !(second == 0xa0 && third == 0x80);
}
Stroke stroke(std::string_view s) {
    if (braille_composite(s)) return {U|R|D|L,5};
    constexpr std::array<std::pair<std::string_view,int>,19> light{{
        {"─",L|R},{"│",U|D},{"┌",R|D},{"┐",L|D},{"└",U|R},{"┘",U|L},
        {"├",U|R|D},{"┤",U|L|D},{"┬",L|R|D},{"┴",U|L|R},{"┼",U|R|D|L},
        {"╭",R|D},{"╮",L|D},{"╰",U|R},{"╯",U|L},{"●",U|R|D|L},{"○",U|R|D|L},
        {"◉",U|R|D|L},{"⊕",U|R|D|L}}};
    for (const auto& [g,b] : light) if (g == s) return {b,1};
    constexpr std::array<std::pair<std::string_view,int>,11> heavy{{
        {"━",L|R},{"┃",U|D},{"┏",R|D},{"┓",L|D},{"┗",U|R},{"┛",U|L},
        {"┣",U|R|D},{"┫",U|L|D},{"┳",L|R|D},{"┻",U|L|R},{"╋",U|R|D|L}}};
    for (const auto& [g,b] : heavy) if (g == s) return {b,2};
    constexpr std::array<std::pair<std::string_view,int>,11> double_box{{
        {"═",L|R},{"║",U|D},{"╔",R|D},{"╗",L|D},{"╚",U|R},{"╝",U|L},
        {"╠",U|R|D},{"╣",U|L|D},{"╦",L|R|D},{"╩",U|L|R},{"╬",U|R|D|L}}};
    for (const auto& [g,b] : double_box) if (g == s) return {b,6};
    if (s == "▁") return {L|R,4};
    if (s == "┄" || s == "┈") return {L|R,3};
    if (s == "┆" || s == "┊") return {U|D,3};
    return {};
}
struct Series { std::string type,name; std::vector<double> values; };
struct XY {
    std::string title,category_title,value_title;
    bool horizontal = false;
    std::optional<std::pair<double,double>> domain;
    std::vector<std::string> categories;
    std::vector<Series> series;
    bool single_bar_axis_marker = false;
    bool shared_axis = false;
    bool singleton_key = false; // Only the separately source-reviewed named-owner profile.
};
struct XYValuePlan {
    int width = 0;
    std::vector<int> bar_overlay;
    std::vector<std::pair<Point,std::string>> tokens;
    std::vector<std::pair<std::size_t,std::size_t>> bindings;
    bool binds(std::size_t owner,std::size_t category,Work& work) const {
        for (const auto& binding : bindings) { work.spend(); if (binding == std::pair{owner,category}) return true; }
        return false;
    }
    bool covers(Point point,std::size_t owner) const {
        const auto at = static_cast<std::size_t>(point.y*width+point.x);
        return point.x >= 0 && point.y >= 0 && point.x < width && at < bar_overlay.size() &&
            bar_overlay[at] == static_cast<int>(owner);
    }
};
// The declared ascending positive domain supplies the physical bar reference.
// It never changes the line map or invents a separately configurable origin.
double xy_bar_reference(const XY& model) {
    return model.domain && model.domain->first > 0 && model.domain->first < model.domain->second
        ? model.domain->first : 0;
}
XY xy_model(JsonVal value,bool explicit_domain) {
    XY m;
    m.title = value.get_string("title"); m.horizontal = value.get("horizontal").as_bool();
    m.category_title = value.get("xAxis").get_string("title"); m.value_title = value.get("yAxis").get_string("title");
    value.get("xAxis").get("categories").iter([&](JsonVal c) { m.categories.emplace_back(c.as_str()); });
    if (explicit_domain) {
        const auto range = value.get("yAxis").get("range");
        m.domain = {{range.get("min").as_double(),range.get("max").as_double()}};
    }
    int bars = 0,lines = 0;
    value.get("series").iter([&](JsonVal s) {
        Series series; series.type = s.get_string("type");
        if (series.type != "bar" && series.type != "line") throw std::runtime_error("Unknown frozen XY series type");
        series.name = (series.type == "bar" ? "Bar " : "Line ") + std::to_string(series.type == "bar" ? ++bars : ++lines);
        s.get("data").iter([&](JsonVal n) { series.values.push_back(n.as_double()); });
        m.series.push_back(std::move(series));
    });
    if (m.categories.empty() || m.categories.size() > 256 || m.series.empty() || m.series.size() > 32)
        throw std::runtime_error("XY model work bounds exceeded");
    for (const auto& s : m.series) {
        if (s.values.size() != m.categories.size()) throw std::runtime_error("XY category/sample count differs");
        for (const auto n : s.values) if (!std::isfinite(n)) throw std::runtime_error("Nonfinite frozen XY value");
    }
    return m;
}
struct Axes { int x = 0,top = 0,y = 0,right = 0; };
std::vector<Axes> axes(const Grid& g,Work& work) {
    std::vector<Axes> result;
    auto bits = [&](int x,int y,int mask) { return (stroke(g.at({x,y}).text).bits & mask) == mask; };
    for (int y = 3; y < g.height; ++y) for (int x = 1; x+3 < g.width; ++x) {
        work.spend();
        if (!bits(x,y,U|R)) continue;
        int top = y,right = x;
        while (top > 0 && bits(x,top,U) && bits(x,top-1,D)) { --top; work.spend(); }
        while (right+1 < g.width && bits(right,y,R) && bits(right+1,y,L)) { ++right; work.spend(); }
        if (y-top >= 3 && right-x >= 3) {
            if (result.size() >= 16) throw std::runtime_error("XY candidate axis-pair bound exceeded");
            result.push_back({x,top,y,right});
        }
    }
    return result;
}
struct Scale {
    double minimum_position = 0,maximum_position = 0,minimum = 0,maximum = 0;
    double project(double n) const {
        return ((n-minimum)/(maximum-minimum))*(maximum_position-minimum_position)+minimum_position;
    }
};
std::optional<Scale> scale(const Grid& g,const Axes& a,bool horizontal,Work& work) {
    std::vector<std::pair<double,double>> ticks;
    if (!horizontal) {
        for (int y = a.top; y <= a.y; ++y) {
            work.spend();
            if (!(stroke(g.at({a.x,y}).text).bits & L)) continue;
            std::string prefix;
            for (int x = 0; x < a.x; ++x) prefix += g.at({x,y}).text;
            const auto ns = numbers(prefix);
            if (!ns.empty()) {
                const auto n = ns.back();
                if (!n.percent && trim(prefix.substr(static_cast<std::size_t>(n.end))).empty()) ticks.emplace_back(n.value,y);
            }
        }
    } else {
        for (int y = a.y+1; y <= std::min(a.y+3,g.height-1); ++y) {
            std::string line;
            for (int x = 0; x < g.width; ++x) line += g.at({x,y}).text;
            // Tick rows contain ASCII numbers only before conversion to cell coordinates.
            // Authored Unicode/category labels are found independently, never byte-indexed.
            if (std::ranges::any_of(line,[](unsigned char c) { return c >= 128; })) continue;
            for (const auto& n : numbers(line)) {
                work.spend();
                const double center = (n.begin+n.end-1)/2.0;
                const int column = static_cast<int>(std::round(center));
                if (!n.percent && column >= a.x && column <= a.right && (stroke(g.at({column,a.y}).text).bits & D))
                    ticks.emplace_back(n.value,center);
            }
            if (ticks.size() >= 2) break;
        }
    }
    if (ticks.size() < 2) return {};
    if (ticks.size() > 64) throw std::runtime_error("XY tick count bound exceeded");
    std::ranges::sort(ticks);
    const auto [lo,position_lo] = ticks.front(); const auto [hi,position_hi] = ticks.back();
    if (!(hi > lo) || !std::isfinite(hi-lo)) return {};
    Scale result{position_lo,position_hi,lo,hi};
    if ((horizontal && position_hi <= position_lo) || (!horizontal && position_hi >= position_lo)) return {};
    for (const auto& [v,p] : ticks) if (std::abs(result.project(v)-p) > 0.51) return {};
    return result;
}
struct Style { Cell sample; int family = 0; std::uint32_t owners = 0; };
struct Legend { std::vector<Style> styles; std::vector<Point> primary; std::vector<int> composite_rows; };
enum class XYCaption { Chart, Category, Value };
bool xy_caption_at(Point p,const XY& m,const Axes& a,XYCaption role,bool native_roles) {
    if (native_roles) {
        if (role == XYCaption::Chart) return p == Point{0,0};
        const bool top = (role == XYCaption::Category) == m.horizontal;
        return p == Point{0,top ? (m.title.empty() ? 0 : 1) : a.y+2};
    }
    if (role == XYCaption::Chart) return p.y < a.top;
    if (role == XYCaption::Category)
        return m.horizontal ? p.x+ftxui::string_width(m.category_title) <= a.x : p.y > a.y;
    return m.horizontal ? p.y > a.y : p.x <= a.x;
}
int xy_native_key_start(const XY& m,const Axes& a) {
    const auto& footer = m.horizontal ? m.value_title : m.category_title;
    return a.y+3+(footer.empty() ? 0 : 1);
}

std::optional<Legend> legend(const Grid& g,const XY& m,const Axes& a,Work& work,bool native_roles = false) {
    Legend result;
    if (m.series.size() == 1 && !m.singleton_key) return result;
    std::vector<std::vector<Point>> names;
    for (const auto& series : m.series) names.push_back(find_text(g,series.name,work));
    for (std::size_t n = 0; n < m.series.size(); ++n) {
        std::vector<Point> matches;
        for (const auto p : names[n]) if (native_roles ? p.y >= xy_native_key_start(m,a) : (p.y > a.y || p.y < a.top || p.x > a.right)) matches.push_back(p);
        if (matches.empty()) return {};
        bool found = false;
        for (const auto p : matches) {
            for (int x = std::max(0,p.x-4); x < p.x; ++x) {
                work.spend(); const auto& c = g.at({x,p.y}); const int family = stroke(c.text).family;
                if (m.singleton_key && m.series[n].type == "line" &&
                    c.text != "─" && c.text != "━" && c.text != "┄" && c.text != "═") continue;
                if ((m.series[n].type == "bar" && fill(c.text)) || (m.series[n].type == "line" && family &&
                    !(native_roles && (braille_composite(c.text) || c.text == "⊕" || c.text == "◉")))) {
                    const auto duplicate = std::ranges::find_if(result.styles,[&](const Style& s) {
                        work.spend(); return s.family == family && same_swatch(s.sample,c);
                    });
                    if (duplicate == result.styles.end()) result.styles.push_back({c,family,std::uint32_t{1} << n});
                    else duplicate->owners |= std::uint32_t{1} << n;
                    result.primary.push_back(p); found = true; break;
                }
            }
            if (found) break;
        }
        if (!found) return {};
    }
    if (native_roles) {
        for (int y = xy_native_key_start(m,a); y < g.height; ++y) {
            work.spend(); const auto& marker = g.at({0,y});
            if (!braille_composite(marker.text) && marker.text != "⊕" && marker.text != "◉") continue;
            std::string row = g.row(y); work.spend(static_cast<std::size_t>(g.width));
            while (!row.empty() && row.back() == ' ') row.pop_back();
            const auto prefix = marker.text+" ";
            if (!row.starts_with(prefix)) return {};
            std::string_view tail(row); tail.remove_prefix(prefix.size());
            std::uint32_t owners = 0; int previous = -1;
            while (true) {
                const auto separator = tail.find(" + ");
                const auto name = tail.substr(0,separator);
                int found = -1;
                for (std::size_t owner = 0; owner < m.series.size(); ++owner) {
                    work.spend();
                    if (m.series[owner].name == name) {
                        if (found >= 0) return {};
                        found = static_cast<int>(owner);
                    }
                }
                if (found <= previous) return {};
                owners |= std::uint32_t{1} << found; previous = found;
                if (separator == std::string_view::npos) break;
                tail.remove_prefix(separator+3);
            }
            if (std::popcount(owners) < 2) return {};
            for (const auto& style : result.styles) {
                work.spend();
                if (std::popcount(style.owners) > 1 &&
                    (style.sample.text == marker.text || style.owners == owners)) return {};
            }
            result.styles.push_back({marker,stroke(marker.text).family,owners});
            result.composite_rows.push_back(y);
        }
    } else {
        // A composite key is an explicit visible mark followed by every owner name
        // on that same row. A legend with two names but only one unkeyed plot mark
        // cannot establish two graphical contributions.
        for (int y = 0; y < g.height; ++y) {
            if (native_roles ? y < xy_native_key_start(m,a) : (y >= a.top && y <= a.y)) continue;
            std::uint32_t owners = 0; bool ambiguous = false;
            for (std::size_t n = 0; n < m.series.size(); ++n) {
                int count = 0;
                for (const auto p : names[n]) if (p.y == y) { owners |= std::uint32_t{1} << n; ++count; }
                if (count > 1) ambiguous = true;
            }
            if (ambiguous || std::popcount(owners) < 2) continue;
            for (int x = 0; x < g.width; ++x) {
                work.spend(); const auto& c = g.at({x,y});
                if (c.text == "◉" || c.text == "⊕") {
                    result.styles.push_back({c,stroke(c.text).family,owners});
                    result.composite_rows.push_back(y);
                }
            }
        }
    }
    for (std::size_t n = 0; n < result.styles.size(); ++n) for (std::size_t q = 0; q < n; ++q) {
        work.spend();
        if (same_swatch(result.styles[n].sample,result.styles[q].sample) && result.styles[n].owners != result.styles[q].owners) return {};
    }
    // Every series must have an individually distinguishable key; a shared
    // swatch alone remains ambiguous, even when a composite key also exists.
    for (std::size_t n = 0; n < m.series.size(); ++n)
        if (!std::ranges::any_of(result.styles,[&](const Style& s) { work.spend(); return s.owners == (std::uint32_t{1} << n); })) return {};
    return result;
}
bool line_style(const Cell& c,const Style& s) {
    const auto a = stroke(c.text),b = stroke(s.sample.text);
    // Endpoint markers have their own literal key, rather than being confused
    // with every light box-drawing stroke.
    if (braille_composite(s.sample.text) || s.sample.text == "●" || s.sample.text == "○" || s.sample.text == "◉" || s.sample.text == "⊕")
        return c.text == s.sample.text && c.foreground == s.sample.foreground && c.background == s.sample.background;
    return a.bits && b.bits && a.family == b.family && c.foreground == s.sample.foreground && c.background == s.sample.background;
}
bool owns(const Cell& c,std::size_t series,const XY& m,const Legend& l,Work& work) {
    if (m.series.size() == 1 && braille_composite(c.text)) return false;
    if (m.series.size() == 1 && !m.singleton_key) return m.series[0].type == "bar" ?
        (fill(c.text) || (m.single_bar_axis_marker && c.text == "⊕")) : stroke(c.text).bits != 0;
    if (m.singleton_key && m.series.size() == 1 && m.series.front().type == "bar" &&
        m.single_bar_axis_marker && c.text == "⊕") return true; // Source endpoint/shaft closure still proves admission.
    bool explicit_key = false; std::uint32_t explicit_owners = 0;
    for (const auto& s : l.styles) {
        work.spend(); if (!same_swatch(c,s.sample)) continue;
        explicit_key = true; explicit_owners |= s.owners;
    }
    // A literal keyed mark has the declared ownership set. Generic light /
    // heavy stroke families must not add a missing composite owner afterward.
    if (explicit_key) return (explicit_owners & (std::uint32_t{1} << series)) != 0;
    if (braille_composite(c.text)) return false;
    for (const auto& s : l.styles) {
        work.spend(); if (!(s.owners & (std::uint32_t{1} << series))) continue;
        if (m.series[series].type == "bar" ? same_swatch(c,s.sample) : line_style(c,s)) return true;
    }
    return false;
}
std::optional<std::vector<int>> anchors(const Grid& g,const XY& m,const Axes& a,Work& work) {
    std::vector<int> result;
    for (const auto& label : m.categories) {
        std::vector<int> positions;
        int nearest_row = g.height;
        const int width = ftxui::string_width(label);
        for (const auto p : find_text(g,label,work)) {
            if (m.horizontal) {
                if (p.x+width <= a.x && p.y >= a.top && p.y < a.y) positions.push_back(p.y);
            } else if (p.y > a.y && p.y <= a.y+3) {
                const int center = p.x+(width-1)/2;
                if (center > a.x && center <= a.right) {
                    if (p.y < nearest_row) { nearest_row = p.y; positions.clear(); }
                    if (p.y == nearest_row) positions.push_back(center);
                }
            }
        }
        if (positions.size() != 1) return {};
        if (!result.empty() && positions.front() <= result.back()) return {};
        result.push_back(positions.front());
    }
    return result;
}
bool exact_disclosure(const Grid& g,const XY& m,std::size_t series,std::size_t category,Work& work) {
    // Text is supplementary: only a row binding the series and category to the
    // numeric value can disambiguate two quantized graphical samples.
    for (const auto p : find_text(g,m.series[series].name,work)) {
        const auto row = g.row(p.y);
        for (const auto q : find_text(g,m.categories[category],work)) if (q.y == p.y) {
            const auto suffix = row.substr(row.find(m.categories[category])+m.categories[category].size());
            if (has_value(suffix,m.series[series].values[category])) return true;
        }
    }
    return false;
}
// Source-derived shared-axis policy. This is a private native display profile,
// not a promise about upstream SVG strokes or midpoint routing.
bool shared_axis_roundtrip_disclosure(const Grid& g,const XY& m,std::size_t owner,std::size_t category,
                                      const Axes& axis,Work& work) {
    work.spend(m.series[owner].name.size()+m.categories[category].size()+2);
    const auto prefix = m.series[owner].name+" "+m.categories[category]+"=";
    const double expected = m.series[owner].values[category],reference = xy_bar_reference(m);
    for (int y = xy_native_key_start(m,axis); y < g.height; ++y) {
        work.spend(static_cast<std::size_t>(g.width)); const auto row = g.row(y);
        if (!row.starts_with(prefix)) continue;
        work.spend(row.size()-prefix.size()+1); const auto token = trim(row.substr(prefix.size()));
        if (!numeric_token(token)) continue;
        const auto values = numbers(token);
        if (values.size() == 1 && !values.front().percent && values.front().begin == 0 &&
            values.front().end == static_cast<int>(token.size()) && values.front().value == expected &&
            values.front().value != reference) return true;
    }
    return false;
}
bool shared_axis_source(const XY& m,Work& work) {
    if (!m.domain || !(m.domain->first > 0 && m.domain->first < m.domain->second)) return false;
    bool bars = false;
    for (const auto& series : m.series) { work.spend(); bars |= series.type == "bar"; }
    if (!bars) return false;
    const double capacity = m.horizontal ? 40 : 20;
    for (const auto& s : m.series) { work.spend(); if (s.type == "line") for (const double value : s.values) {
        work.spend(); const double fraction = (value-m.domain->first)/(m.domain->second-m.domain->first);
        if (fraction >= 0 && fraction <= 1 && std::round(m.horizontal ? capacity*fraction : capacity*(1-fraction)) == (m.horizontal ? 0 : capacity)) return true;
    }}
    return false;
}
struct SharedAxisPlan {
    std::vector<int> axis_bits,data_bits,verified_path_bits;
    std::vector<std::uint32_t> owners,terminal_owners;
    std::string pending;
};
SharedAxisPlan shared_axis_plan(const Grid& g,const XY& m,const Axes& a,const Scale& scale,
                                const std::vector<int>& categories,Work& work) {
    work.spend(g.cells.size()*5);
    SharedAxisPlan plan{std::vector<int>(g.cells.size()),std::vector<int>(g.cells.size()),std::vector<int>(g.cells.size()),
        std::vector<std::uint32_t>(g.cells.size()),std::vector<std::uint32_t>(g.cells.size()),{}};
    auto index = [&](Point p) { return static_cast<std::size_t>(p.y*g.width+p.x); };
    auto axis_segment = [&](Point first,Point last) {
        const Point step{(last.x > first.x)-(last.x < first.x),(last.y > first.y)-(last.y < first.y)};
        const int forward = step.x > 0 ? R : D,reverse = step.x > 0 ? L : U;
        for (Point p = first;; p = {p.x+step.x,p.y+step.y}) {
            work.spend(); if (!g.has(p)) throw std::runtime_error("Source axis exceeds captured canvas");
            if (!(p == first)) plan.axis_bits[index(p)] |= reverse;
            if (!(p == last)) plan.axis_bits[index(p)] |= forward;
            if (p == last) break;
        }
    };
    axis_segment({a.x,a.top},{a.x,a.y}); axis_segment({a.x,a.y},{a.right,a.y});
    for (int tick = 0; tick < 5; ++tick) {
        work.spend(); const Point p = m.horizontal ? Point{a.x+tick*10,a.y} : Point{a.x,a.y-tick*5};
        if (!g.has(p)) throw std::runtime_error("Source axis tick exceeds captured canvas");
        plan.axis_bits[index(p)] |= m.horizontal ? L|R|D : U|D|L;
    }
    plan.axis_bits[index({a.x,a.y})] |= U|R|D|L;
    for (const int category : categories) {
        work.spend(); const Point p = m.horizontal ? Point{a.x,category} : Point{category,a.y};
        if (!g.has(p)) throw std::runtime_error("Source category port exceeds captured canvas");
        plan.axis_bits[index(p)] |= m.horizontal ? U|D|R : L|R|D;
    }
    // A visible straight cap has both collinear stroke arms, even when the
    // source segment has just one inward connection at its endpoint.
    for (auto& bits : plan.axis_bits) {
        work.spend();
        if (bits == U || bits == D) bits = U|D;
        else if (bits == L || bits == R) bits = L|R;
    }
    int lines = 0;
    for (std::size_t owner = 0; owner < m.series.size(); ++owner) {
        const auto& series = m.series[owner]; if (series.type != "line") continue;
        ++lines;
        auto point = [&](std::size_t n) {
            work.spend();
            if (n >= categories.size() || !std::isfinite(series.values[n]) || series.values[n] < scale.minimum || series.values[n] > scale.maximum)
                throw std::runtime_error("Source shared-axis sample cannot be safely projected");
            const int value = static_cast<int>(std::round(scale.project(series.values[n])));
            return m.horizontal ? Point{value,categories[n]} : Point{categories[n],value};
        };
        if (series.values.size() < 2) {
            plan.pending = "Shared axis singleton source path has no reviewed connected-pair geometry"; continue;
        }
        for (const std::size_t n : {std::size_t{0},series.values.size()-1}) {
            work.spend(); plan.terminal_owners[index(point(n))] |= std::uint32_t{1} << owner;
        }
        auto segment = [&](Point first,Point last) {
            const Point step{(last.x > first.x)-(last.x < first.x),(last.y > first.y)-(last.y < first.y)};
            const int forward = step.x > 0 ? R : step.x < 0 ? L : step.y > 0 ? D : U;
            const int reverse = step.x > 0 ? L : step.x < 0 ? R : step.y > 0 ? U : D;
            for (Point p = first;; p = {p.x+step.x,p.y+step.y}) {
                work.spend(); if (!g.has(p)) throw std::runtime_error("Source line exceeds captured canvas");
                const auto at = index(p);
                if (plan.axis_bits[at]) {
                    plan.owners[at] |= std::uint32_t{1} << owner;
                    if (!(p == first)) plan.data_bits[at] |= reverse;
                    if (!(p == last)) plan.data_bits[at] |= forward;
                }
                if (p == last) break;
            }
        };
        for (std::size_t n = 1; n < series.values.size(); ++n) {
            const auto first = point(n-1),last = point(n);
            const int middle = m.horizontal ? (first.y+last.y)/2 : (first.x+last.x)/2;
            const Point one = m.horizontal ? Point{first.x,middle} : Point{middle,first.y};
            const Point two = m.horizontal ? Point{last.x,middle} : Point{middle,last.y};
            segment(first,one); segment(one,two); segment(two,last);
        }
    }
    if (lines > 2) plan.pending = "Shared axis has more than two distinct admitted primary line families";
    for (const auto mask : plan.owners) {
        work.spend(); if (std::popcount(mask) > 1) plan.pending = "Shared axis has an unimplemented multiowner data/axis union";
    }
    return plan;
}
void shared_axis_closure(const Grid& g,const XY& m,const Legend& keys,const SharedAxisPlan& plan,
                        const Claims& claims,Work& work,std::vector<std::string>& errors) {
    if (!plan.pending.empty()) { errors.push_back(plan.pending); return; }
    for (std::size_t at = 0; at < plan.axis_bits.size(); ++at) {
        work.spend(); if (!plan.axis_bits[at]) continue;
        const auto actual = stroke(g.cells[at].text);
        if (!plan.owners[at]) {
            if (actual.family != 6 || actual.bits != plan.axis_bits[at] || claims.data_owners[at])
                errors.push_back("Shared axis-only cell changed exact double-frame ports or borrowed data ownership");
            continue;
        }
        const auto owner = static_cast<std::size_t>(std::countr_zero(plan.owners[at]));
        int family = 0;
        for (std::size_t n = 0; n <= owner; ++n) { work.spend(); family += m.series[n].type == "line"; }
        if (claims.data_owners[at] != plan.owners[at] || (family != 1 && family != 2) || actual.family != family ||
            actual.bits != (plan.axis_bits[at]|plan.data_bits[at]) || !owns(g.cells[at],owner,m,keys,work))
            errors.push_back("Shared axis/data union lacks exact source ports, primary owner, midpoint or verified path");
    }
}

void shared_axis_path_closure(const Grid& g,const XY& m,const Legend& keys,const SharedAxisPlan& plan,
                             const Claims& claims,Work& work,std::vector<std::string>& errors) {
    for (std::size_t at = 0; at < g.cells.size(); ++at) {
        work.spend(); const auto mask = claims.data_owners[at];
        if (!mask || plan.axis_bits[at]) continue;
        const auto& cell = g.cells[at];
        if (std::popcount(mask) > 1) {
            bool keyed = false;
            for (const auto& key : keys.styles) {
                work.spend();
                if (key.owners == mask && same_swatch(cell,key.sample) &&
                    (braille_composite(cell.text) || cell.text == "⊕" || cell.text == "◉")) keyed = true;
            }
            if (!keyed) errors.push_back("Shared-axis profile intersection has no exact abstract composite owner key");
            continue;
        }
        const auto owner = static_cast<std::size_t>(std::countr_zero(mask));
        if (m.series[owner].type != "line") continue;
        int family = 0;
        for (std::size_t n = 0; n <= owner; ++n) { work.spend(); family += m.series[n].type == "line"; }
        int bits = plan.verified_path_bits[at];
        // A real first/last sample uses a straight visible cap. No bend/tee,
        // internal sample, axis, or owner intersection borrows this exception.
        if (plan.terminal_owners[at] & mask) {
            if (bits == L || bits == R) bits = L|R;
            else if (bits == U || bits == D) bits = U|D;
        }
        const auto drawn = stroke(cell.text);
        if (!bits || drawn.family != family || drawn.bits != bits)
            errors.push_back("Shared-axis line glyph has unverified arms or a borrowed source endpoint cap");
    }
}

// Generic captures have no fixed twenty-cell capacity or five-tick grid. Their
// independently calibrated axes and actual selected source paths define roles.
struct GenericStrokePlan {
    std::vector<int> axis_bits,verified_path_bits,bar_ports;
    std::vector<std::uint32_t> terminal_owners,sample_owners,bar_mark_owners;
};
GenericStrokePlan generic_stroke_plan(const Grid& g,const XY& m,const Axes& a,const Scale& scale,
                                      const std::vector<int>& categories,Work& work) {
    work.spend(g.cells.size()*6);
    GenericStrokePlan plan{std::vector<int>(g.cells.size()),std::vector<int>(g.cells.size()),
        std::vector<int>(g.cells.size()),std::vector<std::uint32_t>(g.cells.size()),
        std::vector<std::uint32_t>(g.cells.size()),std::vector<std::uint32_t>(g.cells.size())};
    auto index = [&](Point p) { return static_cast<std::size_t>(p.y*g.width+p.x); };
    auto segment = [&](Point first,Point last) {
        const Point step{(last.x > first.x)-(last.x < first.x),(last.y > first.y)-(last.y < first.y)};
        const int forward = step.x ? R : D,reverse = step.x ? L : U;
        for (Point p = first;; p = {p.x+step.x,p.y+step.y}) {
            work.spend();
            if (!(p == first)) plan.axis_bits[index(p)] |= reverse;
            if (!(p == last)) plan.axis_bits[index(p)] |= forward;
            if (p == last) break;
        }
    };
    segment({a.x,a.top},{a.x,a.y}); segment({a.x,a.y},{a.right,a.y});
    // A frame is a separate complete reciprocal role, never three suggestive
    // corner glyphs or an arbitrary reachable line masquerading as an axis.
    bool frame = (stroke(g.at({a.x,a.top}).text).bits & R) &&
        (stroke(g.at({a.right,a.top}).text).bits & D) && (stroke(g.at({a.right,a.y}).text).bits & U);
    if (frame) {
        for (int x = a.x; x < a.right; ++x) {
            work.spend();
            if (!(stroke(g.at({x,a.top}).text).bits & R) || !(stroke(g.at({x+1,a.top}).text).bits & L)) frame = false;
        }
        for (int y = a.top; y < a.y; ++y) {
            work.spend();
            if (!(stroke(g.at({a.right,y}).text).bits & D) || !(stroke(g.at({a.right,y+1}).text).bits & U)) frame = false;
        }
    }
    if (frame) { segment({a.x,a.top},{a.right,a.top}); segment({a.right,a.top},{a.right,a.y}); }
    // Only complete numeric tokens agreeing with the previously established
    // affine map establish tick ports; extra raw tee arms are not tick facts.
    if (!m.horizontal) {
        for (int y = a.top; y <= a.y; ++y) {
            work.spend(static_cast<std::size_t>(a.x)); std::string prefix;
            for (int x = 0; x < a.x; ++x) prefix += g.at({x,y}).text;
            for (const auto& token : numbers(prefix)) {
                work.spend();
                if (!token.percent && trim(prefix.substr(static_cast<std::size_t>(token.end))).empty() &&
                    std::abs(scale.project(token.value)-y) <= 0.51) plan.axis_bits[index({a.x,y})] |= L;
            }
        }
    } else {
        for (int y = a.y+1; y <= std::min(g.height-1,a.y+3); ++y) {
            work.spend(static_cast<std::size_t>(g.width)); const auto row = g.row(y);
            if (std::ranges::any_of(row,[](unsigned char c) { return c >= 128; })) continue;
            for (const auto& token : numbers(row)) {
                work.spend(); const double center = (token.begin+token.end-1)/2.0;
                const int x = static_cast<int>(std::round(center));
                if (!token.percent && x >= a.x && x <= a.right && std::abs(scale.project(token.value)-center) <= 0.51)
                    plan.axis_bits[index({x,a.y})] |= D;
            }
        }
    }
    plan.axis_bits[index({a.x,a.y})] |= U|R|D|L;
    for (const int category : categories) {
        work.spend(); const Point p = m.horizontal ? Point{a.x,category} : Point{category,a.y};
        plan.axis_bits[index(p)] |= m.horizontal ? R : D;
    }
    for (std::size_t at = 0; at < plan.axis_bits.size(); ++at) {
        work.spend(); auto& bits = plan.axis_bits[at];
        if (bits == U || bits == D) bits = U|D;
        else if (bits == L || bits == R) bits = L|R;
        const Point p{static_cast<int>(at%static_cast<std::size_t>(g.width)),static_cast<int>(at/static_cast<std::size_t>(g.width))};
        const bool corner = frame && (p.x == a.x || p.x == a.right) && (p.y == a.top || p.y == a.y);
        // Numeric/category ticks at an unframed segment extremity retain
        // collinear caps; true closed-frame corners never borrow that rule.
        if (!corner && p.x == a.x && (p.y == a.top || p.y == a.y) && (bits & (L|R))) bits |= U|D;
        if (!corner && p.y == a.y && (p.x == a.x || p.x == a.right) && (bits & (U|D))) bits |= L|R;
    }
    for (std::size_t owner = 0; owner < m.series.size(); ++owner) {
        const auto& series = m.series[owner]; if (series.type != "line") continue;
        for (std::size_t n = 0; n < series.values.size(); ++n) {
            work.spend(); const auto value = series.values[n];
            if (n >= categories.size() || !std::isfinite(value) || value < scale.minimum || value > scale.maximum) continue;
            const int projected = static_cast<int>(std::round(scale.project(value)));
            const Point p = m.horizontal ? Point{projected,categories[n]} : Point{categories[n],projected};
            if (!g.has(p)) continue;
            plan.sample_owners[index(p)] |= std::uint32_t{1} << owner;
            if (series.values.size() > 1 && (!n || n+1 == series.values.size()))
                plan.terminal_owners[index(p)] |= std::uint32_t{1} << owner;
        }
    }
    return plan;
}
void generic_stroke_closure(const Grid& g,const XY& m,const Axes& a,const Legend& keys,const GenericStrokePlan& plan,
                            const Claims& claims,Work& work,std::vector<std::string>& errors) {
    std::optional<Cell> singleton_style;
    for (std::size_t at = 0; at < g.cells.size(); ++at) {
        work.spend(); const int axis = plan.axis_bits[at],path = plan.verified_path_bits[at];
        const auto mask = claims.data_owners[at]; const auto& cell = g.cells[at]; const auto drawn = stroke(cell.text);
        const bool atomic = braille_composite(cell.text) || cell.text == "●" || cell.text == "○" || cell.text == "◉" || cell.text == "⊕";
        if (plan.bar_mark_owners[at]) {
            if (mask != plan.bar_mark_owners[at]) errors.push_back("Atomic bar mark borrowed another data owner");
            continue;
        }
        std::uint32_t lines = 0;
        for (std::size_t owner = 0; owner < m.series.size(); ++owner) {
            work.spend(); if ((mask & (std::uint32_t{1} << owner)) && m.series[owner].type == "line") lines |= std::uint32_t{1} << owner;
        }
        if (atomic && mask && !lines) {
            bool keyed = false;
            for (const auto& key : keys.styles) {
                work.spend(); if (std::popcount(mask) > 1 && same_swatch(cell,key.sample) && key.owners == mask) keyed = true;
            }
            if (!keyed || axis) errors.push_back("Atomic bar stroke is not its validated source point/shaft or exact composite key");
            continue;
        }
        if (!axis && !lines) {
            const Point p{static_cast<int>(at%static_cast<std::size_t>(g.width)),static_cast<int>(at/static_cast<std::size_t>(g.width))};
            if (claims.cells[at] && !mask && drawn.bits && p.x >= a.x && p.x <= a.right && p.y >= a.top && p.y <= a.y &&
                (p.y == a.top || p.x == a.right)) errors.push_back("Claimed frame stroke has no complete reciprocal axis role");
            continue;
        }
        if (atomic) {
            bool keyed = false;
            for (const auto& key : keys.styles) {
                work.spend(); if (same_swatch(cell,key.sample) && key.owners == mask &&
                    (braille_composite(key.sample.text) || key.sample.text == "●" || key.sample.text == "○" || key.sample.text == "◉" || key.sample.text == "⊕")) keyed = true;
            }
            // A singleton light line can use a literal sample dot, and legacy
            // single-family axis overlap uses an explicit owned ⊕ symbol.
            const bool sample_dot = m.series.size() == 1 && lines && (plan.sample_owners[at] & lines) &&
                (cell.text == "●" || cell.text == "○");
            const bool axis_mark = axis && path && lines && cell.text == "⊕" && std::popcount(mask) == 1;
            if (!lines || (!path && !(plan.sample_owners[at] & lines)) || (!keyed && !sample_dot && !axis_mark))
                errors.push_back("Literal line mark lacks exact source point/path/owner identity");
            continue;
        }
        int required = axis|path;
        if (lines && !axis) {
            if (std::popcount(lines) > 1) { errors.push_back("Structural line junction borrowed an abstract composite identity"); continue; }
            const auto owner = static_cast<std::size_t>(std::countr_zero(lines));
            if (!path && m.series[owner].values.size() == 1 && (plan.sample_owners[at] & lines)) required = m.horizontal ? U|D : L|R;
            else if ((plan.terminal_owners[at] & lines) && (path == U || path == D)) required = U|D;
            else if ((plan.terminal_owners[at] & lines) && (path == L || path == R)) required = L|R;
        }
        const int permitted = required|plan.bar_ports[at];
        const bool bits_match = required && (drawn.bits & required) == required && (drawn.bits & ~permitted) == 0;
        bool family = drawn.family == 1;
        if (lines) {
            family = drawn.family == 1 || drawn.family == 2 || drawn.family == 3 || drawn.family == 6;
            for (std::size_t owner = 0; owner < m.series.size(); ++owner) {
                work.spend(); if ((lines & (std::uint32_t{1} << owner)) && !owns(cell,owner,m,keys,work)) family = false;
            }
        }
        if (lines && m.series.size() == 1) {
            if (!singleton_style) singleton_style = cell;
            else if (drawn.family != stroke(singleton_style->text).family || cell.foreground != singleton_style->foreground ||
                     cell.background != singleton_style->background) family = false;
        }
        if (!bits_match || !family) errors.push_back("Ordinary XY stroke has unverified axis/data arms, family or source endpoint cap");
    }
}

bool observable_line(const Grid& g,Point p,const Axes& a,std::size_t series,const XY& m,const Legend& l,Work& work,const SharedAxisPlan* shared = nullptr) {
    if (!owns(g.at(p),series,m,l,work)) return false;
    if (shared && g.has(p)) {
        work.spend(); const auto at = static_cast<std::size_t>(p.y*g.width+p.x);
        if (shared->axis_bits[at]) return (shared->owners[at] & (std::uint32_t{1} << series)) != 0 &&
            (stroke(g.at(p).text).family == 1 || stroke(g.at(p).text).family == 2);
    }
    const bool frame = (stroke(g.at({a.x,a.top}).text).bits & R) &&
        (stroke(g.at({a.right,a.top}).text).bits & D) && (stroke(g.at({a.right,a.y}).text).bits & U);
    const bool boundary = p.y == a.y || p.x == a.x || (frame && (p.y == a.top || p.x == a.right));
    if (!boundary) return true;
    const auto& c = g.at(p); const auto& axis = g.at({a.x,a.y});
    const bool marker = braille_composite(c.text) || c.text == "●" || c.text == "○" || c.text == "◉" || c.text == "⊕";
    return marker || c.foreground != axis.foreground || c.background != axis.background ||
        stroke(c.text).family != stroke(axis.text).family;
}
bool connected(const Grid& g,Point start,Point end,const Axes& a,std::size_t series,const XY& m,const Legend& l,Work& work,Claims& claims,SharedAxisPlan* shared = nullptr,
               std::vector<int>* selected_bits = nullptr,bool bounded_pair = true) {
    constexpr std::array<Point,4> steps{{{0,-1},{1,0},{0,1},{-1,0}}};
    constexpr std::array<int,4> bits{{U,R,D,L}};
    work.spend(g.cells.size()); // Account for the O(canvas) parent initialization.
    std::vector<int> parent(g.cells.size(),-1); std::vector<Point> todo{start};
    const int first = start.y*g.width+start.x;
    parent[static_cast<std::size_t>(first)] = first;
    for (std::size_t n = 0; n < todo.size(); ++n) {
        const auto p = todo[n];
        if (p == end) {
            int current = end.y*g.width+end.x;
            while (true) {
                work.spend(); claims.data({current%g.width,current/g.width},series,work);
                if (current == first) return true;
                const int previous = parent[static_cast<std::size_t>(current)];
                if (selected_bits) {
                    work.spend(2);
                    const Point here{current%g.width,current/g.width},before{previous%g.width,previous/g.width};
                    const int bits = before.x > here.x ? R : before.x < here.x ? L : before.y > here.y ? D : U;
                    const int back = before.x > here.x ? L : before.x < here.x ? R : before.y > here.y ? U : D;
                    (*selected_bits)[static_cast<std::size_t>(current)] |= bits;
                    (*selected_bits)[static_cast<std::size_t>(previous)] |= back;
                }
                current = previous;
            }
        }
        for (std::size_t d = 0; d < steps.size(); ++d) {
            work.spend(); const Point q{p.x+steps[d].x,p.y+steps[d].y};
            if (q.x < a.x || (!shared && q.x == a.x) || q.x > a.right || q.y < a.top || q.y > a.y) continue;
            if (bounded_pair) {
                if (q.x < std::min(start.x,end.x) || q.x > std::max(start.x,end.x) ||
                    q.y < std::min(start.y,end.y) || q.y > std::max(start.y,end.y)) continue;
                const int from = m.horizontal ? start.y : start.x, to = m.horizontal ? end.y : end.x;
                const int current = m.horizontal ? p.y : p.x, next = m.horizontal ? q.y : q.x;
                if ((to > from && next < current) || (to < from && next > current) || (to == from && next != current)) continue;
            }
            const auto index = static_cast<std::size_t>(q.y*g.width+q.x);
            if (parent[index] >= 0 || !observable_line(g,q,a,series,m,l,work,shared)) continue;
            if (!(stroke(g.at(p).text).bits & bits[d]) || !(stroke(g.at(q).text).bits & bits[(d+2)%4])) continue;
            parent[index] = p.y*g.width+p.x; todo.push_back(q);
        }
    }
    return false;
}
void xy_text_claims(const Grid& g,const XY& m,const Axes& a,const Scale& s,const std::vector<int>& category,
                    const Legend& keys,Claims& claims,Work& work,bool native_roles = false) {
    claims.sentinel(work);
    for (int y = a.top; y <= a.y; ++y) claims.cell({a.x,y});
    for (int x = a.x; x <= a.right; ++x) claims.cell({x,a.y});
    const bool frame = (stroke(g.at({a.x,a.top}).text).bits & R) &&
        (stroke(g.at({a.right,a.top}).text).bits & D) && (stroke(g.at({a.right,a.y}).text).bits & U);
    if (frame) {
        for (int x = a.x; x <= a.right; ++x) claims.cell({x,a.top});
        for (int y = a.top; y <= a.y; ++y) claims.cell({a.right,y});
    }
    for (std::size_t n = 0; n < m.categories.size(); ++n) {
        const auto& label = m.categories[n]; const int width = ftxui::string_width(label);
        for (const auto p : find_text(g,label,work)) {
            if ((m.horizontal && p.y == category[n] && p.x+width <= a.x) ||
                (!m.horizontal && p.y > a.y && p.y <= a.y+3 && p.x+(width-1)/2 == category[n])) claims.text(p,label);
        }
    }
    auto title = [&](const std::string& text,auto belongs) {
        if (text.empty()) return;
        for (const auto p : find_text(g,text,work)) if (belongs(p)) claims.text(p,text);
    };
    title(m.title,[&](Point p) { return xy_caption_at(p,m,a,XYCaption::Chart,native_roles); });
    title(m.category_title,[&](Point p) { return xy_caption_at(p,m,a,XYCaption::Category,native_roles); });
    title(m.value_title,[&](Point p) { return xy_caption_at(p,m,a,XYCaption::Value,native_roles); });
    if (!m.horizontal) {
        for (int y = a.top; y <= a.y; ++y) {
            if (!(stroke(g.at({a.x,y}).text).bits & L)) continue;
            std::string prefix; for (int x = 0; x < a.x; ++x) prefix += g.at({x,y}).text;
            const auto ns = numbers(prefix);
            if (!ns.empty()) {
                const auto n = ns.back();
                if (!n.percent && std::abs(s.project(n.value)-y) <= 0.51 && trim(prefix.substr(static_cast<std::size_t>(n.end))).empty())
                    for (int x = n.begin; x < n.end; ++x) claims.cell({x,y});
            }
        }
    } else {
        for (int y = a.y+1; y <= std::min(g.height-1,a.y+3); ++y) {
            const auto line = g.row(y);
            if (std::ranges::any_of(line,[](unsigned char c) { return c >= 128; })) continue;
            for (const auto& n : numbers(line)) {
                const double center = (n.begin+n.end-1)/2.0;
                const int column = static_cast<int>(std::round(center));
                if (!n.percent && column >= a.x && column <= a.right && (stroke(g.at({column,a.y}).text).bits & D) && std::abs(s.project(n.value)-center) <= 0.51)
                    for (int x = n.begin; x < n.end; ++x) claims.cell({x,y});
            }
        }
    }
    for (std::size_t n = 0; n < m.series.size(); ++n) {
        for (const auto p : find_text(g,m.series[n].name,work)) {
            if (p.y >= a.top && p.y <= a.y && p.x <= a.right) continue;
            const bool primary = n < keys.primary.size() && p == keys.primary[n];
            // This source profile does not request values. Its one exact key
            // cannot authorize supplementary readouts through known numbers.
            if (m.singleton_key && !primary) continue;
            const bool composite_row = std::ranges::find(keys.composite_rows,p.y) != keys.composite_rows.end();
            // A supplementary raw-value row is admissible only when it has
            // at least one correctly bound category=value token below.
            bool disclosure_row = false;
            for (std::size_t c = 0; c < m.categories.size(); ++c) for (const auto q : find_text(g,m.categories[c],work)) {
                if (q.y != p.y || q.x < p.x+ftxui::string_width(m.series[n].name)) continue;
                std::string suffix;
                for (int x = q.x+ftxui::string_width(m.categories[c]); x < g.width; ++x) suffix += g.at({x,p.y}).text;
                const auto ns = numbers(suffix);
                if (!ns.empty() && !ns.front().percent && exact(ns.front().value,m.series[n].values[c])) disclosure_row = true;
            }
            if (!primary && !composite_row && !disclosure_row) continue;
            claims.text(p,m.series[n].name);
            for (int x = std::max(0,p.x-4); x < p.x; ++x)
                for (const auto& key : keys.styles) { work.spend(); if ((key.owners & (std::uint32_t{1} << n)) && same_swatch(g.at({x,p.y}),key.sample)) claims.cell({x,p.y}); }
            bool composite = false;
            for (int x = 0; x < g.width; ++x) for (const auto& key : keys.styles) {
                work.spend(); if (std::popcount(key.owners) > 1 && same_swatch(g.at({x,p.y}),key.sample)) { claims.cell({x,p.y}); composite = true; }
            }
            if (composite) for (int x = 0; x < g.width; ++x) if (g.at({x,p.y}).text == "+") claims.cell({x,p.y});
            for (std::size_t c = 0; c < m.categories.size(); ++c) for (const auto q : find_text(g,m.categories[c],work)) {
                if (q.y != p.y || q.x < p.x+ftxui::string_width(m.series[n].name)) continue;
                const int start = q.x+ftxui::string_width(m.categories[c]);
                std::string suffix; for (int x = start; x < g.width; ++x) suffix += g.at({x,p.y}).text;
                const auto ns = numbers(suffix); if (ns.empty()) continue;
                const auto number = ns.front();
                if (number.percent || !exact(number.value,m.series[n].values[c])) continue;
                bool separator = true;
                for (int x = 0; x < number.begin; ++x) if (suffix[static_cast<std::size_t>(x)] != ' ' && suffix[static_cast<std::size_t>(x)] != '=' && suffix[static_cast<std::size_t>(x)] != ':') separator = false;
                if (!separator) continue;
                claims.text(q,m.categories[c]);
                for (int x = start; x < start+number.end; ++x) claims.cell({x,p.y});
            }
        }
    }
}
std::vector<std::string> xy_candidate_errors(const Grid& g,const XY& m,const Axes& a,Work& work,bool native_roles = false,const XYValuePlan* value_labels = nullptr) {
    std::vector<std::string> errors;
    const auto calibrated = scale(g,a,m.horizontal,work);
    if (!calibrated) return {"XY numeric ticks do not establish a monotone affine scale"};
    const auto& s = *calibrated;
    if (!native_roles) work.spend(g.cells.size()); // Claims cell initialization; native wrappers already charge it.
    Claims claims(g);
    claims.enable_data(work);
    if (m.domain && (!exact(s.minimum,m.domain->first) || !exact(s.maximum,m.domain->second))) errors.push_back("Explicit XY domain changed");
    const auto categories = anchors(g,m,a,work);
    if (!categories) return {"XY categories missing, reordered, ambiguous or bound to wrong axis"};
    const auto keys = legend(g,m,a,work,native_roles);
    if (!keys) return {"XY series keys missing or indistinguishable"};
    std::optional<SharedAxisPlan> shared;
    if (m.shared_axis || value_labels) {
        for (const auto& series : m.series) {
            work.spend();
            if (series.values.size() != categories->size()) return {"Shared axis source category/sample count changed"};
            for (const double value : series.values) {
                work.spend();
                if (!std::isfinite(value) || value < s.minimum || value > s.maximum)
                    return {"Shared axis source sample cannot be projected inside its disclosed domain"};
            }
        }
        shared = shared_axis_plan(g,m,a,s,*categories,work);
        if (!shared->pending.empty()) return {shared->pending};
    }
    auto* shared_plan = shared ? &*shared : nullptr;
    std::optional<GenericStrokePlan> generic;
    if (!shared_plan) generic = generic_stroke_plan(g,m,a,s,*categories,work);
    auto* selected_bits = shared_plan ? &shared_plan->verified_path_bits : &generic->verified_path_bits;
    auto title_count = [&](const std::string& text,auto belongs) {
        if (text.empty()) return;
        int count = 0; for (const auto p : find_text(g,text,work)) if (belongs(p)) ++count;
        if (count != 1) errors.push_back("XY authored title missing/ambiguous or bound to wrong axis");
    };
    title_count(m.title,[&](Point p) { return xy_caption_at(p,m,a,XYCaption::Chart,native_roles); });
    title_count(m.category_title,[&](Point p) { return xy_caption_at(p,m,a,XYCaption::Category,native_roles); });
    title_count(m.value_title,[&](Point p) { return xy_caption_at(p,m,a,XYCaption::Value,native_roles); });
    xy_text_claims(g,m,a,s,*categories,*keys,claims,work,native_roles);
    if (value_labels) for (const auto& [at,text] : value_labels->tokens) { work.spend(text.size()); claims.text(at,text); }
    for (const auto& series : m.series) for (const auto value : series.values)
        if (value < s.minimum || value > s.maximum) errors.push_back("XY axis excludes source data");
    for (std::size_t series = 0; series < m.series.size(); ++series) {
        std::vector<Point> samples;
        const auto& values = m.series[series].values;
        for (std::size_t n = 0; n < values.size(); ++n) {
            if (!std::isfinite(values[n]) || values[n] < s.minimum || values[n] > s.maximum) {
                errors.push_back("XY sample cannot be projected inside the disclosed scale"); continue;
            }
            const double projected = s.project(values[n]);
            const int value = static_cast<int>(std::round(projected));
            if (std::abs(projected-value) > 0.51) { errors.push_back("XY sample exceeds cell quantization tolerance"); continue; }
            const int category = (*categories)[n];
            const Point sample = m.horizontal ? Point{value,category} : Point{category,value};
            if (m.series[series].type == "line") {
                if (shared_plan && shared_plan->axis_bits[static_cast<std::size_t>(sample.y*g.width+sample.x)] &&
                    values[n] != xy_bar_reference(m) && !shared_axis_roundtrip_disclosure(g,m,series,n,a,work))
                    errors.push_back("Quantized shared-axis line sample lacks exact owner-bound source value");
                if (!observable_line(g,sample,a,series,m,*keys,work,shared_plan)) errors.push_back("XY line sample missing/wrong owner: " + m.series[series].name + "/" + m.categories[n]);
                else { samples.push_back(sample); claims.data(sample,series,work); }
            } else {
                const double reference = xy_bar_reference(m);
                if (reference < s.minimum || reference > s.maximum) { errors.push_back("XY bars have no disclosed source baseline"); continue; }
                const int baseline = static_cast<int>(std::round(s.project(reference)));
                if (native_roles && reference > 0 && value == baseline && values[n] != reference) {
                    errors.push_back("XY positive-domain tiny bar has no approved category-axis mark"); continue;
                }
                const int previous = n ? ((*categories)[n-1]+category)/2+1 : (m.horizontal ? a.top : a.x+1);
                const int next = n+1 < values.size() ? (category+(*categories)[n+1])/2 : (m.horizontal ? a.y-1 : a.right);
                const int step = value >= baseline ? 1 : -1;
                auto valid_column = [&](int column) {
                    work.spend();
                    if (value == baseline && values[n] != reference) {
                        const Point mark = m.horizontal ? Point{baseline,column} : Point{column,baseline};
                        if (!owns(g.at(mark),series,m,*keys,work) || !exact_disclosure(g,m,series,n,work)) return false;
                    }
                    for (int p = baseline+step; p != value+step && value != baseline; p += step) {
                        const Point point = m.horizontal ? Point{p,column} : Point{column,p};
                        work.spend();
                        if (!owns(g.at(point),series,m,*keys,work) && !(value_labels && value_labels->covers(point,series))) {
                            bool shared_endpoint = p == value;
                            if (shared_endpoint) shared_endpoint = std::ranges::any_of(m.series,[&](const Series& other) {
                                const auto index = static_cast<std::size_t>(&other-m.series.data());
                                return other.type == "line" && observable_line(g,point,a,index,m,*keys,work,shared_plan);
                            });
                            if (!shared_endpoint) return false;
                        }
                    }
                    const int beyond = value+step;
                    const Point outside = m.horizontal ? Point{beyond,column} : Point{column,beyond};
                    return !owns(g.at(outside),series,m,*keys,work);
                };
                int found = previous;
                while (found <= next && !valid_column(found)) ++found;
                if (found > next) errors.push_back("XY bar endpoint/span/ownership wrong: " + m.series[series].name + "/" + m.categories[n]);
                else {
                    // A bar can be several cells thick, but only its validated
                    // contiguous rectangle is owned. A separated island or a
                    // partial second column is never swept into the category.
                    int left = found,right = found;
                    while (left > previous && valid_column(left-1)) --left;
                    while (right < next && valid_column(right+1)) ++right;
                    const int lo = std::min(value,baseline),hi = std::max(value,baseline);
                    for (int column = left; column <= right; ++column) for (int p = lo; p <= hi; ++p) {
                        work.spend(); const Point point = m.horizontal ? Point{p,column} : Point{column,p};
                        if (owns(g.at(point),series,m,*keys,work)) {
                            claims.data(point,series,work);
                            if (generic) {
                                const auto at = static_cast<std::size_t>(point.y*g.width+point.x);
                                const bool tiny = value == baseline && values[n] != reference;
                                const bool negative_marker = m.single_bar_axis_marker && !m.horizontal && values[n] < reference &&
                                    value == a.y && value-baseline > 1 && p == value;
                                if ((tiny && g.at(point).text == "▁") || (negative_marker && g.at(point).text == "⊕"))
                                    generic->bar_mark_owners[at] |= std::uint32_t{1} << series;
                            }
                        }
                        // The baseline junction itself is not a bar fill.
                        // Only the independently validated adjacent real fill
                        // proves an optional structural bar/axis union arm.
                        if (generic && !native_roles && value != baseline && (m.horizontal ? point.x == a.x : point.y == a.y)) {
                            const int adjacent = p == baseline ? p+step : p-step;
                            const Point shaft = m.horizontal ? Point{adjacent,column} : Point{column,adjacent};
                            if (adjacent >= lo && adjacent <= hi && fill(g.at(shaft).text) && owns(g.at(shaft),series,m,*keys,work)) {
                                const auto at = static_cast<std::size_t>(point.y*g.width+point.x);
                                generic->bar_ports[at] |= m.horizontal ? (adjacent > p ? R : L) : (adjacent > p ? D : U);
                            }
                        }
                    }
                }
            }
            for (std::size_t other_series = 0; other_series <= series; ++other_series) {
                const auto& others = m.series[other_series].values;
                for (std::size_t other = 0; other < others.size(); ++other) {
                    work.spend();
                    if (other_series == series && other >= n) break;
                    if (others[other] >= s.minimum && others[other] <= s.maximum &&
                        !exact(values[n],others[other]) && std::round(s.project(others[other])) == value &&
                        (!(value_labels ? value_labels->binds(series,n,work) : exact_disclosure(g,m,series,n,work)) ||
                         !(value_labels ? value_labels->binds(other_series,other,work) : exact_disclosure(g,m,other_series,other,work))))
                        errors.push_back("Quantized XY values lack exact owner-bound disclosure");
                }
            }
        }
        if (m.series[series].type == "line" && samples.size() == values.size())
            for (std::size_t n = 1; n < samples.size(); ++n)
                if (!connected(g,samples[n-1],samples[n],a,series,m,*keys,work,claims,shared_plan,selected_bits,true)) errors.push_back("XY line path disconnected or wrong owner");
    }
    if (native_roles) {
        work.spend(keys->styles.size()); std::vector<bool> used(keys->styles.size());
        for (int y = a.top; y <= a.y; ++y) for (int x = a.x; x <= a.right; ++x) {
            work.spend(); const auto& cell = g.at({x,y}); bool keyed = false;
            for (std::size_t n = 0; n < keys->styles.size(); ++n) {
                work.spend(); const auto& style = keys->styles[n];
                if (std::popcount(style.owners) < 2 || !same_swatch(cell,style.sample)) continue;
                keyed = true; used[n] = true;
                const auto actual = claims.data_owners[static_cast<std::size_t>(y*g.width+x)];
                if (actual != style.owners) errors.push_back("XY composite has missing/invented verified data owners");
            }
            if (braille_composite(cell.text) && !keyed) errors.push_back("XY plot composite has no exact visible owner key");
        }
        for (std::size_t n = 0; n < keys->styles.size(); ++n) {
            work.spend();
            if (std::popcount(keys->styles[n].owners) > 1 && !used[n]) errors.push_back("XY composite legend has no actual data mark");
        }
    }
    if (generic) generic_stroke_closure(g,m,a,*keys,*generic,claims,work,errors);
    if (shared_plan) {
        if (m.shared_axis) shared_axis_closure(g,m,*keys,*shared_plan,claims,work,errors);
        else if (value_labels) for (std::size_t at = 0; at < shared_plan->axis_bits.size(); ++at) {
            work.spend(); if (!shared_plan->axis_bits[at]) continue;
            const auto drawn = stroke(g.cells[at].text);
            if (drawn.family != 1 || drawn.bits != shared_plan->axis_bits[at] ||
                shared_plan->owners[at] || claims.data_owners[at])
                errors.push_back("Value-label ordinary axis changed exact source ports or borrowed data ownership");
        }
        shared_axis_path_closure(g,m,*keys,*shared_plan,claims,work,errors);
    }
    if (!claims.all_claimed(work)) errors.push_back("XY contains unclaimed series/point/branch/text/plot marks");
    return errors;
}
std::vector<std::string> xy_errors(const Grid& g,const XY& m) {
    bounded(g); Work work;
    auto any = [&](std::string_view title) { return title.empty() || !find_text(g,title,work).empty(); };
    if (!any(m.title) || !any(m.category_title) || !any(m.value_title)) return {"XY chart/axis title missing"};
    std::vector<std::string> best{"XY has no connected orthogonal axes"};
    for (const auto a : axes(g,work)) {
        auto errors = xy_candidate_errors(g,m,a,work);
        if (errors.empty()) return {};
        if (best.size() == 1 && best[0] == "XY has no connected orthogonal axes") best = std::move(errors);
        else if (errors.size() < best.size()) best = std::move(errors);
    }
    return best;
}
std::string explain(const std::vector<std::string>& errors) {
    std::string s;
    for (const auto& error : errors) s += error + '\n';
    return s;
}
TEST(MermaidChartsOracle, PieBarsUseOneTotalAndOwnerBoundReadouts) {
    const Pie model{"Share",false,{{"Big",75},{"Small",25}}};
    const auto good = scene({"Share", "      ███████████████░░░░░", "  Big┃███████████████ 75.0%", "Small┃░░░░░ 25.0%"});
    ASSERT_TRUE(pie_errors(good,model).empty()) << explain(pie_errors(good,model));
    auto wrong = good;
    wrong.edit({10,2}).text = " "; // A hole cannot be hidden by correct labels.
    EXPECT_FALSE(pie_errors(wrong,model).empty());
    wrong = good; wrong.edit({20,2}).text = " "; // Overview fixes B=20.
    EXPECT_FALSE(pie_errors(wrong,model).empty());
    auto changed_total = model; changed_total.slices[0].value = 60;
    EXPECT_FALSE(pie_errors(good,changed_total).empty());
    auto changed_owner = model; std::swap(changed_owner.slices[0].label,changed_owner.slices[1].label);
    EXPECT_FALSE(pie_errors(good,changed_owner).empty());
    auto reordered = model; std::ranges::reverse(reordered.slices);
    EXPECT_FALSE(pie_errors(good,reordered).empty());
    wrong = good; write(wrong,{22,2},"70.0%");
    EXPECT_FALSE(pie_errors(wrong,model).empty());
    EXPECT_FALSE(pie_errors(scene({"Share", "Big 75 75.0%", "Small 25 25.0%"}),model).empty());
    EXPECT_FALSE(pie_errors(scene({"Share", "Big┃█ 75.0%", "Small┃░ 25.0%"}),model).empty());
    const auto scaled = scene({"Share", "  Big┃██████ 75.0%", "Small┃░░ 25.0%"});
    EXPECT_TRUE(pie_errors(scaled,model).empty()) << explain(pie_errors(scaled,model));
}
TEST(MermaidChartsOracle, PieHalfTiesAndGenuineLowResolutionRemainExplicit) {
    const Pie unequal{"",false,{{"A",75},{"B",25}}};
    EXPECT_FALSE(pie_errors(scene({"A┃█ 75.0%", "B┃░ 25.0%"}),unequal).empty());
    const Pie collapsed{"",false,{{"A",60},{"B",40}}};
    const auto disclosed = scene({"A┃█ 60.0% (60)", "B┃░ 40.0% (40)"});
    EXPECT_TRUE(pie_errors(disclosed,collapsed).empty()) << explain(pie_errors(disclosed,collapsed));
    EXPECT_FALSE(pie_errors(scene({"A┃█ 60.0%", "B┃░ 40.0%"}),collapsed).empty());
    const Pie exact_values{"",true,{{"Calcium",42.96},{"Iron",5}}};
    const auto show_data = scene({"Calcium┃█████████ 89.6% (42.96)", "   Iron┃░ 10.4% (5)"});
    EXPECT_TRUE(pie_errors(show_data,exact_values).empty()) << explain(pie_errors(show_data,exact_values));
    EXPECT_FALSE(pie_errors(scene({"Calcium┃█████████ 89.6%", "   Iron┃░ 10.4%"}),exact_values).empty());
}
TEST(MermaidChartsOracle, PiePreservesSevenSlicesLongLegendAndEmptyState) {
    const Pie seven{"",false,{{"Item 0",1},{"Item 1",2},{"Item 2",3},{"Item 3",4},{"Item 4",5},{"Item 5",6},{"Item 6",7}}};
    const auto g = scene({"Item 0┃█ 3.6%", "Item 1┃░░ 7.1%", "Item 2┃▒▒▒ 10.7%", "Item 3┃▓▓▓▓ 14.3%",
        "Item 4┃▉▉▉▉▉ 17.9%", "Item 5┃▊▊▊▊▊▊ 21.4%", "Item 6┃▋▋▋▋▋▋▋ 25.0%"});
    EXPECT_TRUE(pie_errors(g,seven).empty()) << explain(pie_errors(g,seven));
    auto dropped = g;
    for (int x = 0; x < dropped.width; ++x) dropped.edit({x,6}).text = " ";
    EXPECT_FALSE(pie_errors(dropped,seven).empty());
    const Pie long_label{"",false,{{"Short",50},{"Very Long Label",50}}};
    EXPECT_TRUE(pie_errors(scene({"          Short┃██ 50.0%", "Very Long Label┃░░ 50.0%"}),long_label).empty());
    EXPECT_TRUE(pie_errors(scene({"No data"}),Pie{}).empty());
    EXPECT_FALSE(pie_errors(scene({"pie"}),Pie{}).empty());
    EXPECT_FALSE(pie_errors(scene({"Fake┃██ 100%"}),Pie{}).empty());
}
TEST(MermaidChartsOracle, UnicodeUsesRealContinuationCellsAndCombiningGraphemes) {
    const Pie model{"图表🙂",false,{{"北京🙂é",50},{"大阪",50}}};
    const auto g = scene({"图表🙂", "北京🙂é┃██ 50.0%", "   大阪┃░░ 50.0%"});
    ASSERT_TRUE(pie_errors(g,model).empty()) << explain(pie_errors(g,model));
    ASSERT_EQ(g.at({1,1}).text,""); // Continuation belonging to 北.
    auto wrong = g; wrong.edit({1,1}).text = " ";
    EXPECT_FALSE(pie_errors(wrong,model).empty());
    wrong = g; wrong.edit({6,1}).text = "e"; // Remove the combining acute mark.
    EXPECT_FALSE(pie_errors(wrong,model).empty());
    auto element = ftxui::vbox({ftxui::text("图表🙂"),ftxui::text("北京🙂é┃██ 50.0%"),ftxui::text("   大阪┃░░ 50.0%")});
    EXPECT_TRUE(pie_errors(capture(element,40,8),model).empty());
    const auto wide = capture(element,100,8);
    EXPECT_TRUE(pie_errors(wide,model).empty());
    (void)capture(element,12,8);
    EXPECT_TRUE(capture(element,100,8) == wide);
}
XY basic_xy(std::string type = "bar",bool horizontal = false) {
    return {"","","",horizontal,{{0,4}},{"A","B"},{{type,type == "bar" ? "Bar 1" : "Line 1",{2,4}}}};
}
Grid vertical_bars() {
    return scene({"4┤       █", "3┤       █", "2┤ █     █", "1┤ █     █", "0┼─┬─────┬─", "   A     B"});
}
TEST(MermaidChartsOracle, XYBarsNeedCalibratedAxesAndCompleteSpans) {
    const auto model = basic_xy(); const auto g = vertical_bars();
    ASSERT_TRUE(xy_errors(g,model).empty()) << explain(xy_errors(g,model));
    auto wrong = g; wrong.edit({3,2}).text = " ";
    EXPECT_FALSE(xy_errors(wrong,model).empty());
    wrong = g; wrong.edit({9,0}).text = " ";
    EXPECT_FALSE(xy_errors(wrong,model).empty());
    wrong = g; wrong.edit({0,0}).text = "5";
    EXPECT_FALSE(xy_errors(wrong,model).empty());
    wrong = g; write(wrong,{3,5},"B"); write(wrong,{9,5},"A");
    EXPECT_FALSE(xy_errors(wrong,model).empty());
    wrong = g; wrong.edit({1,3}).text = " ";
    EXPECT_FALSE(xy_errors(wrong,model).empty());
    EXPECT_FALSE(xy_errors(scene({"A | 2", "B | 4"}),model).empty());
    const auto horizontal = scene({" A ├██", "   │", " B ├████", "   │", "   ┼─┬─┬", "   0 2 4"});
    EXPECT_TRUE(xy_errors(horizontal,basic_xy("bar",true)).empty()) << explain(xy_errors(horizontal,basic_xy("bar",true)));
    EXPECT_FALSE(xy_errors(horizontal,model).empty());
    auto zero = model; zero.series[0].values = {0,0};
    const auto zero_grid = scene({"4┤", "3┤", "2┤", "1┤", "0┼─┬─────┬─", "   A     B"});
    EXPECT_TRUE(xy_errors(zero_grid,zero).empty()) << explain(xy_errors(zero_grid,zero));
    EXPECT_FALSE(xy_errors(zero_grid,model).empty());
}
TEST(MermaidChartsOracle, XYLineNeedsOrderedSamplesAndConnectedStrokes) {
    auto model = basic_xy("line"); model.series[0].values = {1,3};
    const auto ascending = scene({"4┤", "3┤ ╭──────", "2┤ │", "1┤ │", "0┼─┬─────┬─", "   A     B"});
    ASSERT_TRUE(xy_errors(ascending,model).empty()) << explain(xy_errors(ascending,model));
    auto wrong = ascending; wrong.edit({3,2}).text = " ";
    EXPECT_FALSE(xy_errors(wrong,model).empty());
    wrong = ascending; wrong.edit({9,1}).text = " "; wrong.edit({9,0}).text = "─";
    EXPECT_FALSE(xy_errors(wrong,model).empty());
    const auto descending = scene({"4┤", "3┤ │", "2┤ │", "1┤ ╰──────", "0┼─┬─────┬─", "   A     B"});
    model.series[0].values = {3,1};
    EXPECT_TRUE(xy_errors(descending,model).empty()) << explain(xy_errors(descending,model));
    const auto constant = scene({"4┤", "3┤", "2┤ ───────", "1┤", "0┼─┬─────┬─", "   A     B"});
    model.series[0].values = {2,2};
    EXPECT_TRUE(xy_errors(constant,model).empty()) << explain(xy_errors(constant,model));
    model.categories = {"A"}; model.series[0].values = {2};
    const auto singleton = scene({"4┤", "3┤", "2┤ ─", "1┤", "0┼─┬───────", "   A"});
    EXPECT_TRUE(xy_errors(singleton,model).empty()) << explain(xy_errors(singleton,model));
    const auto horizontal = scene({" A ├ ──╮", "   │   │", " B ├   │", "   │", "   ┼─┬─┬", "   0 2 4"});
    EXPECT_TRUE(xy_errors(horizontal,basic_xy("line",true)).empty()) << explain(xy_errors(horizontal,basic_xy("line",true)));
}
TEST(MermaidChartsOracle, XYGroupedBarsAndMixedOverlapRetainEverySeriesOwner) {
    auto bars = basic_xy(); bars.series = {{"bar","Bar 1",{1,3}},{"bar","Bar 2",{2,4}}};
    const auto grouped = scene({"4┤       ░", "3┤      █░", "2┤  ░   █░", "1┤ █░   █░", "0┼─┬─────┬─", "   A     B", "", "█ Bar 1", "░ Bar 2"});
    ASSERT_TRUE(xy_errors(grouped,bars).empty()) << explain(xy_errors(grouped,bars));
    auto wrong = grouped;
    for (auto& c : wrong.cells) if (c.text == "░") c.text = "█";
    EXPECT_FALSE(xy_errors(wrong,bars).empty());
    auto mixed = basic_xy(); mixed.series.push_back({"line","Line 1",{2,4}});
    const auto shared = scene({"4┤ ╭──────", "3┤ │     █", "2┤ │     █", "1┤ █     █", "0┼─┬─────┬─", "   A     B", "", "█ Bar 1", "─ Line 1"});
    EXPECT_TRUE(xy_errors(shared,mixed).empty()) << explain(xy_errors(shared,mixed));
    wrong = shared; wrong.edit({3,3}).text = " ";
    EXPECT_FALSE(xy_errors(wrong,mixed).empty());
    wrong = shared; wrong.edit({3,1}).text = " ";
    EXPECT_FALSE(xy_errors(wrong,mixed).empty());
}
TEST(MermaidChartsOracle, XYCoincidentLinesRequireAnExplicitCompositeOwnershipKey) {
    auto model = basic_xy("line"); model.series = {{"line","Line 1",{2,2}},{"line","Line 2",{2,2}}};
    const auto shared = scene({"4┤", "3┤", "2┤ ⊕⊕⊕⊕⊕⊕⊕", "1┤", "0┼─┬─────┬─", "   A     B", "", "─ Line 1", "━ Line 2", "⊕ Line 1 + Line 2"});
    ASSERT_TRUE(xy_errors(shared,model).empty()) << explain(xy_errors(shared,model));
    auto wrong = shared;
    for (int x = 3; x <= 9; ++x) wrong.edit({x,2}).text = "─";
    EXPECT_FALSE(xy_errors(wrong,model).empty());
    wrong = shared;
    for (int x = 0; x < wrong.width; ++x) wrong.edit({x,9}).text = " ";
    EXPECT_FALSE(xy_errors(wrong,model).empty());
}
TEST(MermaidChartsOracle, XYColorKeysAreMeasuredFromActualCellStyles) {
    auto model = basic_xy("line"); model.series = {{"line","Line 1",{1,1}},{"line","Line 2",{3,3}}};
    auto colored = scene({"4┤", "3┤ ───────", "2┤", "1┤ ───────", "0┼─┬─────┬─", "   A     B", "", "─ Line 1", "─ Line 2"});
    for (int x = 3; x <= 9; ++x) { colored.edit({x,3}).foreground = "red"; colored.edit({x,1}).foreground = "blue"; }
    colored.edit({0,7}).foreground = "red"; colored.edit({0,8}).foreground = "blue";
    ASSERT_TRUE(xy_errors(colored,model).empty()) << explain(xy_errors(colored,model));
    auto wrong = colored;
    for (int x = 3; x <= 9; ++x) wrong.edit({x,1}).foreground = "red";
    EXPECT_FALSE(xy_errors(wrong,model).empty());
}
TEST(MermaidChartsOracle, RejectsUnknownSliceSeriesPointsBranchesAndDuplicatePlots) {
    const Pie p{"",false,{{"A",75},{"B",25}}};
    EXPECT_FALSE(pie_errors(scene({"A┃███ 75.0%", "B┃░ 25.0%", "Unknown┃■■ 50%"}),p).empty());
    EXPECT_FALSE(pie_errors(scene({"A┃███ 75.0%", "B┃░ 25.0%", "123.45"}),p).empty());
    auto bars = vertical_bars();
    bars.edit({6,1}).text = "█";
    EXPECT_FALSE(xy_errors(bars,basic_xy()).empty());
    auto in_window = vertical_bars(); in_window.edit({5,3}).text = "█";
    EXPECT_FALSE(xy_errors(in_window,basic_xy()).empty());
    auto partial_column = vertical_bars(); partial_column.edit({4,3}).text = "█";
    EXPECT_FALSE(xy_errors(partial_column,basic_xy()).empty());
    auto model = basic_xy("line"); model.series[0].values = {1,3};
    const auto branched = scene({"4┤    │", "3┤ ╭──┴───", "2┤ │", "1┤ ╰", "0┼─┬─────┬─", "   A     B"});
    EXPECT_FALSE(xy_errors(branched,model).empty());
    const auto duplicate = scene({"4┤       █", "3┤       █", "2┤ █     █", "1┤ █     █", "0┼─┬─────┬─", "   A     B", "",
        "4┤       █", "3┤       █", "2┤ █     █", "1┤ █     █", "0┼─┬─────┬─", "   A     B"});
    EXPECT_FALSE(xy_errors(duplicate,basic_xy()).empty());
    const auto extra_series = scene({"4┤       █", "3┤       █", "2┤ █     █", "1┤ █     █", "0┼─┬─────┬─", "   A     B", "", "░ Bar 2"});
    EXPECT_FALSE(xy_errors(extra_series,basic_xy()).empty());
    auto grouped_model = basic_xy(); grouped_model.series = {{"bar","Bar 1",{1,3}},{"bar","Bar 2",{2,4}}};
    const auto duplicate_key = scene({"4┤       ░", "3┤      █░", "2┤  ░   █░", "1┤ █░   █░", "0┼─┬─────┬─", "   A     B", "", "█ Bar 1", "░ Bar 2", "█ Bar 1"});
    EXPECT_FALSE(xy_errors(duplicate_key,grouped_model).empty());
    auto titled_model = basic_xy(); titled_model.title = "My Chart";
    const auto duplicate_title = scene({"My Chart", "My Chart", "4┤       █", "3┤       █", "2┤ █     █", "1┤ █     █", "0┼─┬─────┬─", "   A     B"});
    EXPECT_FALSE(xy_errors(duplicate_title,titled_model).empty());
    EXPECT_FALSE(pie_errors(scene({"No data", "123"}),Pie{}).empty());
}
TEST(MermaidChartsOracle, SubCellNonzeroBarsNeedVisibleDataInkAndExactDisclosure) {
    auto model = basic_xy(); model.series[0].values = {0.1,0.1};
    const auto invisible = scene({"4┤", "3┤", "2┤", "1┤", "0┼─┬─────┬─", "   A     B", "", "Bar 1 A=0.1 B=0.1"});
    EXPECT_FALSE(xy_errors(invisible,model).empty());
    auto visible = invisible; visible.edit({3,4}).text = "▁"; visible.edit({9,4}).text = "▁";
    ASSERT_TRUE(xy_errors(visible,model).empty()) << explain(xy_errors(visible,model));
    auto undisclosed = visible;
    for (int x = 0; x < undisclosed.width; ++x) undisclosed.edit({x,7}).text = " ";
    EXPECT_FALSE(xy_errors(undisclosed,model).empty());
    auto zeros = model; zeros.series[0].values = {0,0};
    EXPECT_FALSE(xy_errors(visible,zeros).empty());
}
TEST(MermaidChartsOracle, PlotFramesAndBaselineAxesCannotImpersonateConstantLines) {
    auto model = basic_xy("line"); model.series[0].values = {4,4};
    const auto frame = scene({"4┌─────────┐", "3┤         │", "2┤         │", "1┤         │", "0┼─┬─────┬─┘", "   A     B"});
    EXPECT_FALSE(xy_errors(frame,model).empty());
    model.series[0].values = {0,0};
    const auto axes_only = scene({"4┤", "3┤", "2┤", "1┤", "0┼─┬─────┬─", "   A     B"});
    EXPECT_FALSE(xy_errors(axes_only,model).empty());
    auto distinct = axes_only;
    for (int x = 3; x <= 9; ++x) distinct.edit({x,4}).text = "━";
    distinct.edit({3,4}).text = "┳"; distinct.edit({9,4}).text = "┳";
    EXPECT_TRUE(xy_errors(distinct,model).empty()) << explain(xy_errors(distinct,model));
}
TEST(MermaidChartsOracle, WorkAndCaptureCapsFailClosed) {
    Work work; work.left = 1;
    EXPECT_NO_THROW(work.spend()); EXPECT_THROW(work.spend(),std::runtime_error);
    const Grid oversized{241,1,std::vector<Cell>(241)};
    EXPECT_THROW(pie_errors(oversized,Pie{}),std::runtime_error);
    EXPECT_THROW(xy_errors(oversized,basic_xy()),std::runtime_error);
}
TEST(MermaidChartsOracle, DistinctCompositeOwnerSetsCannotReuseOneVisibleKey) {
    auto model = basic_xy("line");
    model.series = {{"line","Line 1",{2,2}},{"line","Line 2",{2,2}},{"line","Line 3",{2,2}}};
    const auto unambiguous = scene({"4┤", "3┤", "2┤ ⊕⊕⊕⊕⊕⊕⊕", "1┤", "0┼─┬─────┬─", "   A     B", "", "─ Line 1", "━ Line 2", "┄ Line 3", "⊕ Line 1 + Line 2 + Line 3"});
    ASSERT_TRUE(xy_errors(unambiguous,model).empty()) << explain(xy_errors(unambiguous,model));
    const auto ambiguous = scene({"4┤", "3┤", "2┤ ⊕⊕⊕⊕⊕⊕⊕", "1┤", "0┼─┬─────┬─", "   A     B", "", "─ Line 1", "━ Line 2", "┄ Line 3", "⊕ Line 1 + Line 2", "⊕ Line 2 + Line 3"});
    EXPECT_FALSE(xy_errors(ambiguous,model).empty());
    const auto missing_owner = scene({"4┤", "3┤", "2┤ ⊕⊕⊕⊕⊕⊕⊕", "1┤", "0┼─┬─────┬─", "   A     B", "", "─ Line 1", "━ Line 2", "┄ Line 3", "⊕ Line 2 + Line 3"});
    EXPECT_FALSE(xy_errors(missing_owner,model).empty());
}
struct Vendor { const char* name; const char* family; const char* sha; std::size_t count; };
constexpr std::array<Vendor,2> kVendors{{
    {"beautiful_mermaid","xychart","4a1fab2e4235ba475f00fe9c687bce0d1cb15f7bacf470c5d287a0ba957a4293",22},
    {"termaid","pie","f828e1939c15c0ea44cec661119f2606a993e4538b531fee59e5b73757170094",14}
}};
std::string read(const std::filesystem::path& p) {
    std::ifstream in(p,std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read " + p.string());
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
std::string digest(std::string_view s) { return loom::utils::crypto::sha256(s); }
template<class Fn> void batch(Fn&& fn) {
    const auto root = std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_terminal";
    std::set<std::string> exact_sources;
    for (const auto& v : kVendors) {
        const auto raw = read(root / v.name / "cases.json");
        if (digest(raw) != v.sha) throw std::runtime_error("Frozen chart input/model/provenance file changed: " + std::string(v.name));
        const auto doc = json::parse(raw);
        if (!doc) throw std::runtime_error("Cannot parse frozen chart cases");
        std::size_t count = 0;
        doc->root().get("cases").iter([&](JsonVal row) {
            if (row.get_string("family") != v.family) return;
            ++count;
            const auto sha = digest(row.get_string("input"));
            if (sha != row.get_string("sha256") || !exact_sources.insert(sha).second)
                throw std::runtime_error("Changed/duplicate exact chart input");
            fn(v,row);
        });
        if (count != v.count) throw std::runtime_error("Frozen complete chart family count changed");
    }
}
bool explicit_domain(std::string_view id) {
    // Reviewed original-source facts: these three inputs explicitly author
    // 0 --> 100. The remaining parser ranges are automatically padded.
    constexpr std::array<std::string_view,3> explicit_ids{{
        "beautiful_mermaid_524835415613fed3","beautiful_mermaid_61908f6b1b1181d7","beautiful_mermaid_7aa8a694720f563d"}};
    return std::ranges::find(explicit_ids,id) != explicit_ids.end();
}
std::string fenced(std::string_view source,std::string_view language) {
    return "```" + std::string(language) + "\n" + std::string(source) + "\n```\n\n" + std::string(kSentinel);
}
struct OriginalAssertions { std::vector<std::string> errors; int checked = 0,foreign_profile = 0,semantic_mapping = 0; };
OriginalAssertions original_assertions(const Grid& g,const Vendor& vendor,JsonVal expectation) {
    OriginalAssertions result;
    const auto flat = g.text();
    if (std::string_view(vendor.name) == "beautiful_mermaid") {
        expectation.get("assertions").iter([&](JsonVal a) {
            if (a.get("render_options").as_bool()) { ++result.foreign_profile; return; }
            const auto kind = a.get_string("kind"),value = a.get_string("value");
            if (kind != "contains" && kind != "not_contains") throw std::runtime_error("Unmapped original Beautiful chart assertion");
            ++result.checked;
            const bool found = flat.find(value) != std::string::npos;
            if ((kind == "contains") != found) result.errors.push_back("Original Unicode assertion failed: " + kind + " " + value);
        });
    } else {
        expectation.get("upstream_output_assertions").iter([&](JsonVal a) {
            // All literal glyph/ASCII padding contracts remain in provenance;
            // native Unicode swatch/separator geometry is checked above. The
            // original ASCII-only function must not impose #/* on Unicode UI.
            if (a.get_string("portability") == "requires_unicode_ui_semantic_adaptation") {
                if (a.get_string("value") == "#" || a.get_string("value") == "*") ++result.foreign_profile;
                else ++result.semantic_mapping;
                return;
            }
            const auto kind = a.get_string("kind"),value = a.get_string("value");
            if (kind != "contains" && kind != "not_contains") throw std::runtime_error("Unmapped original Termaid pie assertion");
            ++result.checked;
            const bool found = flat.find(value) != std::string::npos;
            if ((kind == "contains") != found) result.errors.push_back("Original pie assertion failed: " + kind + " " + value);
        });
    }
    return result;
}
TEST(MermaidChartsOracle, CompleteFrozenFamilyInventoryIsIndependentOfRenderer) {
    std::size_t xy = 0,pie = 0,empty = 0;
    ASSERT_NO_THROW(batch([&](const Vendor& v,JsonVal row) {
        const auto expectation = row.get("expectation");
        if (std::string_view(v.family) == "xychart") {
            ++xy;
            const auto model = xy_model(expectation.get("semantic_model"),explicit_domain(row.get_string("id")));
            EXPECT_FALSE(model.series.empty());
        } else {
            ++pie;
            const auto model = pie_model(expectation.get("upstream_semantic_model"));
            if (model.slices.empty()) ++empty;
        }
    }));
    EXPECT_EQ(xy,22u); EXPECT_EQ(pie,14u); EXPECT_EQ(empty,1u);
}
void source_fallback(std::string_view source) {
    auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
    auto ordinary = loom::ui::render_markdown(fenced(source,"text"));
    const auto wide = capture(element,240,256);
    EXPECT_TRUE(same_source_surface(wide,capture(ordinary,240,256))) << "Incomplete/unsupported chart must preserve its full source surface";
    EXPECT_TRUE(same_source_surface(capture(element,40,256),capture(ordinary,40,256)));
    EXPECT_TRUE(capture(element,240,256) == wide);
}
TEST(MermaidChartsResources, EmptyWithTitleAndQuotedWhitespaceKeepAuthoredMeaning) {
    const auto empty_source = std::string("pie\ntitle Authored Title");
    auto empty = loom::ui::render_markdown(fenced(empty_source,"mermaid"));
    const auto empty_grid = capture(empty,240,256);
    EXPECT_FALSE(same_source_surface(empty_grid,capture(loom::ui::render_markdown(fenced(empty_source,"text")),240,256)));
    EXPECT_TRUE(pie_errors(empty_grid,Pie{"Authored Title",false,{}}).empty());
    const std::string spaced_source = "pie\n  \"  A \" : 75\n  \"B\" : 25";
    const auto spaced = capture(loom::ui::render_markdown(fenced(spaced_source,"mermaid")),240,256);
    const Pie expected{"",false,{{"  A ",75},{"B",25}}};
    EXPECT_TRUE(pie_errors(spaced,expected).empty()) << explain(pie_errors(spaced,expected));
    const std::string xy_source = "xychart-beta\nx-axis [\"  A \", B]\nbar [1, 2]";
    const XY xy{"","","",false,std::nullopt,{"A","B"},{{"bar","Bar 1",{1,2}}}};
    const auto xy_grid = capture(loom::ui::render_markdown(fenced(xy_source,"mermaid")),240,256);
    EXPECT_TRUE(xy_errors(xy_grid,xy).empty()) << explain(xy_errors(xy_grid,xy));
}
TEST(MermaidChartsResources, NarrowFiniteDomainsNeverProjectAnExcludedZero) {
    const std::string source = "xychart-beta\nx-axis [a, b]\ny-axis 1 --> 1.0000000000000002\nline [1, 1.0000000000000002]";
    const XY expected{"","","",false,{{1,1.0000000000000002}},{"a","b"},{{"line","Line 1",{1,1.0000000000000002}}}};
    const auto grid = capture(loom::ui::render_markdown(fenced(source,"mermaid")),240,256);
    EXPECT_TRUE(xy_errors(grid,expected).empty()) << explain(xy_errors(grid,expected));
    const std::string negative = "xychart-beta\nx-axis [a, b]\ny-axis -1.0000000000000002 --> -1\nline [-1.0000000000000002, -1]";
    const XY negative_expected{"","","",false,{{-1.0000000000000002,-1}},{"a","b"},{{"line","Line 1",{-1.0000000000000002,-1}}}};
    const auto negative_grid = capture(loom::ui::render_markdown(fenced(negative,"mermaid")),240,256);
    EXPECT_TRUE(xy_errors(negative_grid,negative_expected).empty()) << explain(xy_errors(negative_grid,negative_expected));
}
TEST(MermaidChartsResources, PreservesWholeSourceForUnsupportedSyntaxAndAmbiguousOwnership) {
    const std::array<std::string_view,12> sources{{
        "pie\n\"A\" : 75\n\"B\" : 25\nunknown_statement",
        "xychart-beta\nx-axis [A, B]\nbar [1, 2]\nunknown_statement",
        "xychart-beta\nx-axis [A, B]\nbar [1]",
        "xychart-beta\nx-axis [A, B]\ny-axis 1 --> 1\nline [1, 1]",
        "xychart-beta\nx-axis [A, B]\ny-axis 0 --> inf\nline [1, 2]",
        "xychart-beta\nx-axis [A, B]\nline [1, 1e999]",
        "pie\n\"<b>A</b>\" : 1",
        "pie\n\"A&amp;B\" : 1",
        "%%{init: {}}%%\npie\n\"A\" : 1",
        "---\nconfig:\npie:\ntextPosition: 0.5\n---\npie\n\"A\" : 1",
        "xychart-beta\nx-axis [a,b,c]\ny-axis 0 --> 10\nline [0,10,0]\nline [0,0,10]\nline [10,0,0]",
        "xychart-beta\nx-axis [a,b]\ny-axis 0 --> 4\nline [0,4]\nline [1,3]"
    }};
    for (const auto source : sources) { SCOPED_TRACE(std::string(source)); source_fallback(source); }
    // Controls/bidi/leading combining text cannot silently disappear in FTXUI.
    source_fallback("pie\n\"A\x1b[31m\" : 1");
    source_fallback("pie\n\"\xE2\x80\xAE" "A\" : 1");
    source_fallback("pie\n\"\xCC\x81" "A\" : 1");
}
TEST(MermaidChartsResources, SourceLabelCanvasAndSwatchLimitsDoNotRenderPrefixes) {
    source_fallback("pie\n\"A\" : 1\n%%" + std::string(65536,'x'));
    source_fallback("pie\n\"" + std::string(257,'x') + "\" : 1");
    source_fallback("pie\n\"" + std::string(256,'x') + "\" : 1"); // The retained label still exceeds the 240-column canvas.
    std::string source = "pie\n"; Pie expected;
    for (int n = 0; n < 16; ++n) {
        const auto label = "Slice"+std::to_string(n);
        source += "\""+label+"\" : 1\n"; expected.slices.push_back({label,1});
    }
    const auto at_limit = capture(loom::ui::render_markdown(fenced(source,"mermaid")),240,256);
    EXPECT_TRUE(pie_errors(at_limit,expected).empty()) << explain(pie_errors(at_limit,expected));
    source += "\"Slice16\" : 1\n";
    source_fallback(source);
}
TEST(MermaidChartsSemantics, CompleteBeautifulXYAndTermaidPieBatch) {
    std::size_t visited = 0,nonempty = 0,empty = 0;
    json::JsonMutDoc report; auto root = report.object(); auto rows = root.make_arr();
    root.set("kind","independent_chart_cell_geometry_observations");
    root.set("expected_nonempty_geometry",35); root.set("expected_empty_state",1);
    root.set("qualification","Original sources/models/assertions were frozen before Loom observation. Fallback, empty state, foreign profile and geometry are separate outcomes; no chart summary counts as XY geometry.");
    ASSERT_NO_THROW(batch([&](const Vendor& vendor,JsonVal row) {
        ++visited; SCOPED_TRACE(row.get_string("id"));
        const auto source = row.get_string("input"),id = row.get_string("id");
        const auto expectation = row.get("expectation");
        const bool is_xy = std::string_view(vendor.family) == "xychart";
        const auto typed = expectation.get(is_xy ? "semantic_model" : "upstream_semantic_model");
        const auto xy = is_xy ? xy_model(typed,explicit_domain(id)) : XY{};
        const auto pie = is_xy ? Pie{} : pie_model(typed);
        const bool empty_state = !is_xy && pie.slices.empty();
        if (empty_state) ++empty; else ++nonempty;
        auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
        auto ordinary = loom::ui::render_markdown(fenced(source,"text"));
        const auto wide = capture(element,240,256);
        const auto fallback = capture(ordinary,240,256);
        const bool source_fallback = same_source_surface(wide,fallback);
        auto errors = is_xy ? xy_errors(wide,xy) : pie_errors(wide,pie);
        if (source_fallback) errors.insert(errors.begin(),"Original chart obligation returned source fallback");
        const auto original = original_assertions(wide,vendor,expectation);
        EXPECT_TRUE(errors.empty()) << explain(errors);
        EXPECT_TRUE(original.errors.empty()) << explain(original.errors);
        const auto narrow = capture(element,40,256);
        const bool narrow_source = same_source_surface(narrow,capture(ordinary,40,256));
        const auto narrow_errors = is_xy ? xy_errors(narrow,xy) : pie_errors(narrow,pie);
        // Narrow screens can remain fully graphical or return the complete
        // source. Neither a clipped partial drawing nor a text summary passes.
        EXPECT_TRUE(narrow_source || narrow_errors.empty()) << explain(narrow_errors);
        const auto restored = capture(element,240,256);
        EXPECT_TRUE(restored == wide) << "Same chart node did not restore its exact cells/styles";
        EXPECT_NE(wide.text().find(kSentinel),std::string::npos);
        EXPECT_NE(narrow.text().find(kSentinel),std::string::npos);
        EXPECT_NE(restored.text().find(kSentinel),std::string::npos);
        auto item = root.make_obj(); item.set("id",id); item.set("family",vendor.family);
        item.set("input_sha256",digest(source)); item.set("semantic_model_sha256",digest(typed.to_string()));
        item.set("expectation",empty_state ? "empty_state" : "geometry_positive");
        item.set("observed",source_fallback ? "source_fallback" : (errors.empty() ? (empty_state ? "empty_state" : "geometry") : "geometry_error"));
        item.set("narrow_source_fallback",narrow_source); item.set("restored_identically",restored == wide);
        item.set("original_assertions_checked",original.checked); item.set("foreign_ascii_profile_assertions",original.foreign_profile);
        item.set("original_glyph_assertions_mapped_to_geometry",original.semantic_mapping);
        item.set("pie_projection",is_xy ? "not_applicable" : "termaid_part_to_whole_bars");
        item.set("pie_textPosition",source.find("textPosition") == std::string::npos ? "absent" : "retained_source_option_not_applicable_to_bar_projection");
        auto strings = [&](std::string_view key,const std::vector<std::string>& values) {
            auto a = root.make_arr(); for (const auto& value : values) a.append(root.make_str(value)); item.add(key,a);
        };
        strings("semantic_errors",errors); strings("original_assertion_errors",original.errors);
        auto cells = [&](std::string_view key,const Grid& grid) {
            std::vector<std::string> lines; for (int y = 0; y < grid.height; ++y) lines.push_back(grid.row(y)); strings(key,lines);
        };
        cells("wide_frame",wide); cells("narrow_frame",narrow); cells("restored_frame",restored);
        rows.append(item);
    }));
    EXPECT_EQ(visited,36u); EXPECT_EQ(nonempty,35u); EXPECT_EQ(empty,1u);
    root.add("cases",rows); report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto p = std::filesystem::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-charts-semantics.json";
    std::ofstream out(p,std::ios::binary);
    ASSERT_TRUE(out && (out << report.to_pretty_string() << '\n')) << "Cannot write " << p;
#endif
}
} // namespace

namespace {
// C1 observation only: complete source execution is not a geometry oracle.
// The separately reviewed C2 profiles must certify every native drawing claim.
template<class Fn> void merman_xy102_observation_batch(Fn&& fn) {
    const auto root = std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_terminal/merman";
    const auto raw = read(root/"cases.json");
    if (digest(raw) != "89ce2ef6ad779481285ce61b771a64fd059b17a824eb2c292934ff13393562db")
        throw std::runtime_error("Frozen complete Merman XY source/model/assertion corpus changed");
    const auto doc = json::parse(raw);
    if (!doc) throw std::runtime_error("Cannot parse complete Merman cases");
    std::set<std::string> sources,models,functions;
    int count = 0,references = 0,applications = 0,snapshots = 0,nonempty = 0;
    doc->root().get("cases").iter([&](JsonVal row) {
        if (row.get_string("family") != "xychart") return;
        ++count;
        const auto source = row.get_string("input");
        if (digest(source) != row.get_string("sha256") || !sources.insert(digest(source)).second)
            throw std::runtime_error("Changed/duplicate exact Merman XY source");
        const auto expected = row.get("expectation");
        auto check_reference = [&](JsonVal ref) {
            const auto bytes = read(root/ref.get_string("path"));
            const auto hash = digest(bytes);
            if (hash != ref.get_string("sha256")) throw std::runtime_error("Merman model reference integrity failed");
            ++references; models.insert(hash);
            return bytes;
        };
        const auto primary = check_reference(expected.get("upstream_semantic_model"));
        const auto model = json::parse(primary);
        if (!model) throw std::runtime_error("Cannot parse pinned Merman XY model");
        bool has_data = false;
        model->root().get("plots").iter([&](JsonVal plot) {
            plot.get("values").iter([&](JsonVal) { has_data = true; });
        });
        nonempty += has_data;
        expected.get("runtime_semantic_model_variants").iter([&](JsonVal ref) { check_reference(ref); });
        expected.get("snapshots").iter([&](JsonVal ref) {
            if (digest(read(root/ref.get_string("path"))) != ref.get_string("sha256"))
                throw std::runtime_error("Merman original output reference integrity failed");
            ++snapshots;
        });
        expected.get("assertions").iter([&](JsonVal ref) {
            if (digest(read(root/ref.get_string("path"))) != ref.get_string("sha256"))
                throw std::runtime_error("Merman original assertion slice integrity failed");
            ++applications; functions.insert(ref.get_string("test_name"));
        });
        fn(row,has_data);
    });
    if (count != 102 || sources.size() != 102 || nonempty != 97 || references != 204 ||
        models.size() != 95 || snapshots != 93 || applications != 108 || functions.size() != 34)
        throw std::runtime_error("Complete Merman XY C1 denominator changed");
}
TEST(MermaidChartsMermanXY102Observation, EveryFrozenSourceRunsWithoutClaimingGeometryCoverage) {
    json::JsonMutDoc report; auto root = report.object(); auto rows = root.make_arr();
    root.set("kind","complete_merman_xy102_C1_observation_not_geometry_coverage");
    root.set("source_facts_sha256","345eacce08f47a1d3cfa6375d8f013c2a94465e29600005dc8748785d944bfbf");
    root.set("original_assertion_map_sha256","b9ddb37076a52d8f9e55c250ea445d1b956cecf17a84aa8bf7da75668e7e230d");
    root.set("qualification","All102 execute. Candidate art and source fallback both remain pending independent C2 geometry/empty-state certification. No original RGB/HTML/private option predicate is counted executed.");
    int visited = 0,candidates = 0,fallbacks = 0,observation_errors = 0;
    ASSERT_NO_THROW(merman_xy102_observation_batch([&](JsonVal row,bool has_data) {
        ++visited; const auto id = row.get_string("id"),source = row.get_string("input"); SCOPED_TRACE(id);
        auto item = root.make_obj(); item.set("id",id); item.set("input_sha256",digest(source));
        item.set("public_obligation",has_data ? "nonempty_data" : "empty_state");
        item.set("native_certification","oracle_profile_pending");
        item.set("upstream_observation",row.get("expectation").get("upstream_observation").get_string("category"));
        try {
            auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
            auto ordinary = loom::ui::render_markdown(fenced(source,"text"));
            const auto wide = capture(element,240,256),plain = capture(ordinary,240,256);
            const bool source_fallback = same_source_surface(wide,plain);
            item.set("observed",source_fallback ? "source_fallback" : "candidate_art_not_certified");
            const auto narrow = capture(element,40,256);
            item.set("narrow_observed",same_source_surface(narrow,capture(ordinary,40,256)) ? "source_fallback" : "candidate_art_not_certified");
            const auto restored = capture(element,240,256);
            item.set("restored_identically",restored == wide);
            EXPECT_TRUE(restored == wide);
            EXPECT_NE(wide.text().find(kSentinel),std::string::npos);
            EXPECT_NE(narrow.text().find(kSentinel),std::string::npos);
            auto frame = root.make_arr();
            for (int y = 0; y < wide.height; ++y) frame.append(root.make_str(wide.row(y)));
            item.add("wide_frame",frame);
            if (source_fallback) ++fallbacks; else ++candidates;
        } catch (const std::exception& error) {
            ++observation_errors; item.set("observed","observation_error"); item.set("error",error.what());
            ADD_FAILURE() << "Merman C1 observation failed: " << error.what();
        }
        rows.append(item);
    }));
    EXPECT_EQ(visited,102); EXPECT_EQ(candidates+fallbacks+observation_errors,102);
    root.set("exact_inputs",visited); root.set("nonempty_data_obligations",97); root.set("empty_state_obligations",5);
    root.set("candidate_art_not_certified",candidates); root.set("source_fallback",fallbacks);
    root.set("observation_error",observation_errors); root.set("oracle_profile_pending",visited);
    root.set("certified_native_drawing_passes",0); root.set("certified_native_empty_passes",0);
    root.set("original_function_case_applications_pending",108); root.add("cases",rows); report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto path = std::filesystem::path(LOOM_MERMAID_REPORT_DIR)/"mermaid-merman-xy102-C1-observation.json";
    std::ofstream out(path,std::ios::binary);
    ASSERT_TRUE(out && (out << report.to_pretty_string() << '\n')) << "Cannot write " << path;
#endif
}
} // namespace

namespace {
// Additive second cohort. First-36 test cases and frozen facts are unchanged.
// Numeric-token matching was corrected locally; renderer output selects no inputs.
struct TermaidXY {
    XY chart;
    std::optional<std::pair<double,double>> x_domain;
    std::vector<double> x_values;
};
std::string numeric_x_label(double value) {
    std::array<char,64> buffer{};
    const auto converted = std::to_chars(buffer.data(),buffer.data()+buffer.size(),value);
    if (converted.ec != std::errc{}) throw std::runtime_error("Cannot encode frozen numeric x coordinate");
    return {buffer.data(),converted.ptr};
}
TermaidXY termaid_xy_model(JsonVal typed) {
    if (typed.get_string("__type__") != "XYChart") throw std::runtime_error("Frozen Termaid XY model kind changed");
    TermaidXY result; auto& m = result.chart;
    m.title = typed.get_string("title"); m.category_title = typed.get_string("x_label");
    m.value_title = typed.get_string("y_label"); m.horizontal = typed.get("horizontal").as_bool();
    typed.get("x_categories").iter([&](JsonVal value) { m.categories.emplace_back(value.as_str()); });
    std::vector<double> y_range,x_range;
    typed.get("y_range").iter([&](JsonVal value) { y_range.push_back(value.as_double()); });
    typed.get("x_range").iter([&](JsonVal value) { x_range.push_back(value.as_double()); });
    if (!y_range.empty()) {
        if (y_range.size() != 2 || !(y_range[1] > y_range[0])) throw std::runtime_error("Invalid frozen Termaid y range");
        m.domain = {{y_range[0],y_range[1]}};
    }
    int bars = 0,lines = 0;
    typed.get("datasets").iter([&](JsonVal row) {
        Series series; series.type = row.get_string("chart_type");
        if (series.type != "bar" && series.type != "line") throw std::runtime_error("Unknown frozen Termaid dataset type");
        series.name = row.get_string("label");
        const int index = series.type == "bar" ? ++bars : ++lines;
        if (series.name.empty()) series.name = (series.type == "bar" ? "Bar " : "Line ")+std::to_string(index);
        row.get("values").iter([&](JsonVal n) {
            const double value = n.as_double();
            if (!std::isfinite(value)) throw std::runtime_error("Nonfinite frozen Termaid value");
            series.values.push_back(value);
        });
        m.series.push_back(std::move(series));
    });
    std::size_t n_points = 0;
    for (const auto& series : m.series) n_points = std::max(n_points,series.values.size());
    if (!x_range.empty()) {
        if (x_range.size() != 2 || !(x_range[1] > x_range[0]) || m.horizontal || !m.categories.empty())
            throw std::runtime_error("Numeric-x second-cohort contract changed");
        result.x_domain = {{x_range[0],x_range[1]}};
        for (std::size_t n = 0; n < n_points; ++n) {
            const double value = x_range[0]+(x_range[1]-x_range[0])*static_cast<double>(n)/static_cast<double>(std::max<std::size_t>(1,n_points-1));
            result.x_values.push_back(value);
            // These are expected visible numeric tick identities from the typed
            // range, never a rewritten categorical Mermaid source. Their actual
            // cell positions must independently satisfy the affine x contract.
            m.categories.push_back(numeric_x_label(value));
        }
    }
    if (m.categories.empty() || m.categories.size() > 256 || m.series.empty() || m.series.size() > 32)
        throw std::runtime_error("Termaid XY model work bounds exceeded");
    for (const auto& series : m.series) if (series.values.size() != m.categories.size())
        throw std::runtime_error("Second Termaid cohort has unexpected ragged datasets");
    typed.get("warnings").iter([&](JsonVal) { throw std::runtime_error("Frozen Termaid parser warning needs independent admission review"); });
    return result;
}
std::vector<std::string> numeric_x_candidate_errors(const Grid& g,const TermaidXY& model,const Axes& a,Work& work) {
    if (!model.x_domain) return {};
    const auto anchors_at = anchors(g,model.chart,a,work);
    if (!anchors_at) return {"Numeric x sample labels are missing, reordered or ambiguous"};
    std::vector<std::pair<double,int>> ticks;
    int tick_row = -1;
    for (int y = a.y+1; y <= std::min(g.height-1,a.y+3); ++y) {
        std::string line = g.row(y);
        if (std::ranges::any_of(line,[](unsigned char c) {
            return c != ' ' && c != '+' && c != '-' && c != '.' && c != 'e' && c != 'E' && (c < '0' || c > '9');
        })) continue;
        const auto values = numbers(line);
        if (values.empty()) continue;
        if (tick_row != -1) return {"Duplicate numeric x tick rows"};
        tick_row = y;
        for (const auto& number : values) {
            work.spend();
            if (ticks.size() >= 64) throw std::runtime_error("Numeric x tick work bound exceeded");
            // The existing categorical oracle uses the lower center cell for
            // even-width labels. The numeric axis must share that visible port.
            const int x = (number.begin+number.end-1)/2;
            if (number.percent || x <= a.x || x > a.right || !(stroke(g.at({x,a.y}).text).bits & D))
                return {"Numeric x tick lacks its actual axis port"};
            ticks.emplace_back(number.value,x);
        }
    }
    if (ticks.size() < 3 || ticks.size() != model.x_values.size()) return {"Numeric x ticks do not cover every source sample"};
    const auto [lo,hi] = *model.x_domain;
    if (!exact(ticks.front().first,lo) || !exact(ticks.back().first,hi)) return {"Authored numeric x endpoints missing or changed"};
    const Scale scale_x{static_cast<double>(ticks.front().second),static_cast<double>(ticks.back().second),lo,hi};
    if (!(scale_x.maximum_position > scale_x.minimum_position)) return {"Numeric x direction or span changed"};
    for (std::size_t n = 0; n < ticks.size(); ++n) {
        work.spend();
        if (!exact(ticks[n].first,model.x_values[n]) || (n && ticks[n].second <= ticks[n-1].second))
            return {"Unexpected, duplicated or reordered numeric x tick"};
        const double position = scale_x.project(ticks[n].first);
        if (!std::isfinite(position) || std::abs(position-ticks[n].second) > 0.500000001 ||
            std::abs(position-(*anchors_at)[n]) > 0.500000001)
            return {"A numeric x tick/sample violates the common affine mapping"};
    }
    return {};
}
std::vector<std::string> termaid_xy_errors(const Grid& g,const TermaidXY& model) {
    bounded(g); Work work;
    // Fixed-size FTXUI captures include trailing blank rows and columns.
    // Inspect every original cell before removing only those empty suffixes; a
    // late extra mark/background must remain in the ownership universe.
    work.spend(g.cells.size());
    const auto default_background = ftxui::Color(ftxui::Color::Default).Print(true);
    int height = 1,width = 1;
    for (int y = 0; y < g.height; ++y) for (int x = 0; x < g.width; ++x) {
        const auto& cell = g.at({x,y});
        if ((!cell.text.empty() && cell.text != " ") ||
            (!cell.background.empty() && cell.background != default_background)) {
            height = y+1;
            width = std::max(width,std::min(g.width,x+std::max(1,ftxui::string_width(cell.text))));
        }
    }
    work.spend(static_cast<std::size_t>(width*height));
    Grid compact{width,height,{}};
    compact.cells.reserve(static_cast<std::size_t>(width*height));
    for (int y = 0; y < height; ++y)
        compact.cells.insert(compact.cells.end(),g.cells.begin()+g.width*y,g.cells.begin()+g.width*y+width);
    // Match the independently valid complete geometry candidate, then account
    // for every numeric x tick. Three good ticks never authorize bad extras.
    std::vector<std::string> best{"XY chart has no complete calibrated geometry"};
    for (const auto& a : axes(compact,work)) {
        auto errors = xy_candidate_errors(compact,model.chart,a,work);
        if (errors.empty()) errors = numeric_x_candidate_errors(compact,model,a,work);
        if (errors.empty()) return {};
        if (best.front() == "XY chart has no complete calibrated geometry" || errors.size() < best.size()) best = std::move(errors);
    }
    return best;
}
TermaidXY handmade_numeric_x() {
    return {XY{"","","",false,{{0,4}},{"0","5","10","15","20"},{{"line","Line 1",{1,2,3,2,1}}}},{{0,20}},{0,5,10,15,20}};
}
Grid handmade_numeric_curve() {
    auto g = scene({"4┤", "3┤", "2┤", "1┤", "0┼─┬───┬───┬───┬───┬─", "   0   5   10  15  20"});
    write(g,{9,1},"╭───╮"); write(g,{5,2},"╭───╯"); write(g,{13,2},"╰───╮");
    write(g,{3,3},"──╯"); write(g,{17,3},"╰──");
    return g;
}
TEST(MermaidChartsXY23Oracle, NumericXNeedsEveryAffineTickAndOrderedSample) {
    const auto model = handmade_numeric_x(); const auto good = handmade_numeric_curve();
    ASSERT_TRUE(termaid_xy_errors(good,model).empty()) << explain(termaid_xy_errors(good,model));
    auto wrong = good; write(wrong,{11,5},"11");
    EXPECT_FALSE(termaid_xy_errors(wrong,model).empty());
    wrong = good; wrong.edit({7,2}).text = " "; wrong.edit({7,1}).text = "─";
    EXPECT_FALSE(termaid_xy_errors(wrong,model).empty());
    wrong = good; wrong.edit({3,5}).text = " ";
    EXPECT_FALSE(termaid_xy_errors(wrong,model).empty());
    wrong = good; wrong.edit({19,5}).text = " "; wrong.edit({20,5}).text = " ";
    EXPECT_FALSE(termaid_xy_errors(wrong,model).empty());
    EXPECT_FALSE(termaid_xy_errors(scene({"x 0 5 10 15 20", "y 1 2 3 2 1"}),model).empty());
}
TEST(MermaidChartsXY23Oracle, NumericXRejectsCategoricalWarpAndExtraTicks) {
    auto model = handmade_numeric_x(); model.chart.series[0].values = {2,2,2,2,2};
    const auto good = scene({"4┤", "3┤", "2┤ ─────────────────", "1┤", "0┼─┬───┬───┬───┬───┬─", "   0   5   10  15  20"});
    ASSERT_TRUE(termaid_xy_errors(good,model).empty()) << explain(termaid_xy_errors(good,model));
    auto warped = good;
    warped.edit({11,4}).text = "─"; warped.edit({12,4}).text = "┬";
    warped.edit({11,5}).text = " "; write(warped,{12,5},"10");
    ASSERT_TRUE(xy_errors(warped,model.chart).empty()) << explain(xy_errors(warped,model.chart));
    EXPECT_FALSE(termaid_xy_errors(warped,model).empty());
    const auto extra = scene({"4┤", "3┤", "2┤ ─────────────────", "1┤", "0┼─┬───┬───┬───┬───┬─", "   0   5   10  15  20", "   0"});
    EXPECT_FALSE(termaid_xy_errors(extra,model).empty());
    auto wrong_domain = model; wrong_domain.x_domain = {{0,25}};
    EXPECT_FALSE(termaid_xy_errors(good,wrong_domain).empty());
}
TEST(MermaidChartsXY23Oracle, NumericXPreservesCoincidentSeriesOwnership) {
    auto model = handmade_numeric_x(); model.chart.categories = {"0","5","10"};
    model.x_domain = {{0,10}}; model.x_values = {0,5,10};
    model.chart.series = {{"line","Line 1",{2,2,2}},{"line","Line 2",{2,2,2}}};
    const auto good = scene({"4┤", "3┤", "2┤ ⊕⊕⊕⊕⊕⊕⊕⊕⊕", "1┤", "0┼─┬───┬───┬─", "   0   5   10", "", "─ Line 1", "━ Line 2", "⊕ Line 1 + Line 2"});
    ASSERT_TRUE(termaid_xy_errors(good,model).empty()) << explain(termaid_xy_errors(good,model));
    auto wrong = good; write(wrong,{2,9},"Line 2         ");
    EXPECT_FALSE(termaid_xy_errors(wrong,model).empty());
    auto swapped = model; swapped.chart.series[1].values = {1,1,1};
    EXPECT_FALSE(termaid_xy_errors(good,swapped).empty());
}
TEST(MermaidChartsXY23Oracle, EmptyCaptureSuffixOptimizationNeverDropsLateInk) {
    const auto small = handmade_numeric_curve(); const auto model = handmade_numeric_x();
    Grid padded{240,256,std::vector<Cell>(240u*256u)};
    for (int y = 0; y < small.height; ++y)
        std::copy_n(small.cells.begin()+y*small.width,small.width,padded.cells.begin()+y*padded.width);
    ASSERT_TRUE(termaid_xy_errors(padded,model).empty()) << explain(termaid_xy_errors(padded,model));
    padded.edit({239,255}).text = "█";
    EXPECT_FALSE(termaid_xy_errors(padded,model).empty());
    padded.edit({239,255}).text = " "; padded.edit({239,255}).background = "nondefault_background_witness";
    EXPECT_FALSE(termaid_xy_errors(padded,model).empty());
}
template<class Fn> void termaid_xy23_batch(Fn&& fn) {
    const auto vendor_root = std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_terminal/termaid";
    const auto raw = read(vendor_root/"cases.json");
    if (digest(raw) != kVendors[1].sha) throw std::runtime_error("Frozen complete Termaid input/model/assertions changed");
    const auto doc = json::parse(raw);
    if (!doc) throw std::runtime_error("Cannot parse complete Termaid cases");
    std::set<std::string> sources,models;
    std::size_t count = 0,snapshots = 0;
    doc->root().get("cases").iter([&](JsonVal row) {
        if (row.get_string("family") != "xychart") return;
        ++count;
        const auto source = row.get_string("input");
        if (digest(source) != row.get_string("sha256") || !sources.insert(digest(source)).second)
            throw std::runtime_error("Changed/duplicate exact Termaid XY source");
        models.insert(digest(row.get("expectation").get("upstream_semantic_model").to_string()));
        row.get("expectation").get("snapshots").iter([&](JsonVal ref) {
            if (digest(read(vendor_root/ref.get_string("path"))) != ref.get_string("sha256"))
                throw std::runtime_error("Frozen original/captured Termaid XY snapshot changed");
            ++snapshots;
        });
        fn(row);
    });
    if (count != 23 || sources.size() != 23 || models.size() != 13 || snapshots != 33)
        throw std::runtime_error("Complete Termaid XY inventory/model/snapshot denominator changed");
}
std::vector<std::string> diagram_content(const Grid& g) {
    std::vector<std::string> result;
    for (int y = 0; y < g.height; ++y) {
        auto line = g.row(y);
        if (line.find(kSentinel) != std::string::npos) break;
        const auto end = line.find_last_not_of(' ');
        result.push_back(end == std::string::npos ? std::string{} : line.substr(0,end+1));
    }
    while (!result.empty() && result.back().empty()) result.pop_back();
    return result;
}
struct XY23OriginalConditions {
    int applications = 0,snapshot_mapping = 0,unicode = 0,dimensions = 0,foreign_ascii = 0,literal_tests = 0;
    OriginalAssertions literals;
    std::vector<std::string> errors;
};
XY23OriginalConditions termaid_xy23_conditions(const Grid& g,JsonVal expectation) {
    XY23OriginalConditions result;
    const auto content = diagram_content(g);
    expectation.get("assertions").iter([&](JsonVal assertion) {
        ++result.applications;
        const auto name = assertion.get_string("test_name");
        if (name == "TestAllDiagramSnapshots.test_snapshot") { ++result.snapshot_mapping; return; }
        if (name == "TestAllDiagramInvariants.test_ascii_mode") { ++result.foreign_ascii; return; }
        if (name == "TestAllDiagramInvariants.test_valid_unicode") {
            ++result.unicode;
            for (const auto& line : content) {
                if (line.find("\xEF\xBF\xBD") != std::string::npos) result.errors.push_back("Original Unicode helper: replacement character present");
                std::string roundtrip;
                for (const auto& glyph : ftxui::Utf8ToGlyphs(line)) roundtrip += glyph;
                if (roundtrip != line) result.errors.push_back("Original Unicode helper: UTF-8 glyph roundtrip lost bytes");
            }
            return;
        }
        if (name == "TestAllDiagramInvariants.test_reasonable_dimensions") {
            ++result.dimensions;
            if (content.size() > 200) result.errors.push_back("Original dimension helper: more than 200 diagram rows");
            for (const auto& line : content) {
                const auto count = std::ranges::count_if(line,[](unsigned char c) { return (c & 0xc0) != 0x80; });
                if (count > 500) result.errors.push_back("Original dimension helper: more than 500 Unicode codepoints");
            }
            return;
        }
        if (name == "TestXYChartRendering.test_bar_chart" || name == "TestXYChartRendering.test_horizontal_bar" || name == "TestXYChartRendering.test_combo") {
            ++result.literal_tests; return;
        }
        throw std::runtime_error("Unmapped original Termaid XY condition");
    });
    result.literals = original_assertions(g,kVendors[1],expectation);
    return result;
}
TEST(MermaidChartsXY23Oracle, CompleteInputAndOriginalConditionInventoryIsFrozen) {
    std::size_t numeric = 0,multi = 0,horizontal = 0,applications = 0;
    std::map<std::string,int> functions;
    const auto root = std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_terminal/termaid";
    EXPECT_EQ(digest(read(root/"upstream/tests/test_snapshots.py")),"2f1cb7a06e8be761d820adc1ae3dc4b1a0faa7815289f2d034f47bf0b2706dc6");
    EXPECT_EQ(digest(read(root/"upstream/tests/test_xychart.py")),"4a01c8088ef0ecae694e78f2f5df68908204dbabdb4ba32fac7778277ba80198");
    const auto helper = read(root/"upstream/tests/conftest.py");
    EXPECT_EQ(digest(helper),"d5aef9566d28167ba5bb8c40ac6e15063d531261bdf5c848fce79bcef1ba4871");
    EXPECT_EQ(digest(std::string_view(helper).substr(3663,4061-3663)),"61b6ea6d63850b8ba8f74060952aaca57c2cfb0185c346a1fd52cffef72efcd7");
    EXPECT_EQ(digest(std::string_view(helper).substr(4063,4525-4063)),"dc84e1468b82675e7d1bc4dcb553610544543d25f8e9028b72d977ef137e3936");
    ASSERT_NO_THROW(termaid_xy23_batch([&](JsonVal row) {
        const auto expected = row.get("expectation"); const auto model = termaid_xy_model(expected.get("upstream_semantic_model"));
        numeric += model.x_domain.has_value(); multi += model.chart.series.size() > 1; horizontal += model.chart.horizontal;
        expected.get("assertions").iter([&](JsonVal a) { ++functions[a.get_string("test_name")]; ++applications; });
    }));
    EXPECT_EQ(numeric,2u); EXPECT_EQ(multi,13u); EXPECT_EQ(horizontal,5u); EXPECT_EQ(applications,43u);
    EXPECT_EQ(functions.size(),7u);
    for (const auto& [name,count] : functions) EXPECT_EQ(count,name.starts_with("TestAllDiagram") ? 10 : 1) << name;
}
TEST(MermaidChartsSemantics, CompleteTermaidXY23Batch) {
    json::JsonMutDoc report; auto root = report.object(); auto rows = root.make_arr();
    root.set("kind","complete_termaid_xy23_independent_cell_geometry");
    root.set("expected_geometry",23); root.set("original_function_case_applications",43);
    root.set("qualification","All raw exact sources execute; foreign ASCII options and exact upstream layout bytes remain separately mapped, not native drawing passes.");
    int visited = 0,drawn = 0,fallbacks = 0,geometry_errors = 0,original_errors = 0,oracle_errors = 0;
    XY23OriginalConditions totals;
    ASSERT_NO_THROW(termaid_xy23_batch([&](JsonVal row) {
        ++visited; const auto id = row.get_string("id"),source = row.get_string("input"); SCOPED_TRACE(id);
        const auto expectation = row.get("expectation"); const auto model = termaid_xy_model(expectation.get("upstream_semantic_model"));
        auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
        auto ordinary = loom::ui::render_markdown(fenced(source,"text"));
        const auto wide = capture(element,240,256),fallback = capture(ordinary,240,256);
        const bool is_fallback = same_source_surface(wide,fallback);
        bool oracle_error = false;
        auto observe = [&](const Grid& grid) {
            try { return termaid_xy_errors(grid,model); }
            catch (const std::exception& error) {
                oracle_error = true;
                return std::vector<std::string>{"Explicit chart oracle error: "+std::string(error.what())};
            }
        };
        auto errors = observe(wide);
        const bool wide_oracle_error = oracle_error;
        if (is_fallback) { ++fallbacks; errors.insert(errors.begin(),"Complete original XY source returned source fallback"); }
        else if (wide_oracle_error) ++oracle_errors;
        else if (!errors.empty()) ++geometry_errors;
        else ++drawn;
        auto originals = termaid_xy23_conditions(wide,expectation);
        originals.errors.insert(originals.errors.end(),originals.literals.errors.begin(),originals.literals.errors.end());
        if (!originals.errors.empty()) ++original_errors;
        totals.applications += originals.applications; totals.snapshot_mapping += originals.snapshot_mapping;
        totals.unicode += originals.unicode; totals.dimensions += originals.dimensions; totals.foreign_ascii += originals.foreign_ascii;
        totals.literal_tests += originals.literal_tests; totals.literals.checked += originals.literals.checked;
        totals.literals.semantic_mapping += originals.literals.semantic_mapping;
        EXPECT_TRUE(errors.empty()) << explain(errors);
        EXPECT_TRUE(originals.errors.empty()) << explain(originals.errors);
        const auto narrow = capture(element,40,256);
        const bool narrow_source = same_source_surface(narrow,capture(ordinary,40,256));
        const auto narrow_errors = observe(narrow);
        EXPECT_TRUE(narrow_source || narrow_errors.empty()) << explain(narrow_errors);
        const auto restored = capture(element,240,256);
        EXPECT_TRUE(restored == wide);
        EXPECT_NE(wide.text().find(kSentinel),std::string::npos);
        EXPECT_NE(narrow.text().find(kSentinel),std::string::npos);
        auto item = root.make_obj(); item.set("id",id); item.set("input_sha256",digest(source));
        item.set("observed",is_fallback ? "source_fallback" : (wide_oracle_error ? "oracle_error" : (errors.empty() ? "geometry" : "geometry_error")));
        item.set("numeric_x",model.x_domain.has_value()); item.set("restored_identically",restored == wide);
        item.set("original_condition_applications",originals.applications);
        item.set("snapshot_equality_mapped_to_native_geometry",originals.snapshot_mapping);
        item.set("unicode_helper_checked",originals.unicode); item.set("dimension_helper_checked",originals.dimensions);
        item.set("foreign_explicit_ascii_profile",originals.foreign_ascii); item.set("literal_assertions_checked",originals.literals.checked);
        item.set("glyph_assertions_mapped_to_geometry",originals.literals.semantic_mapping);
        auto strings = [&](std::string_view key,const std::vector<std::string>& values) {
            auto values_json = root.make_arr(); for (const auto& value : values) values_json.append(root.make_str(value)); item.add(key,values_json);
        };
        strings("semantic_errors",errors); strings("original_assertion_errors",originals.errors);
        std::vector<std::string> lines; for (int y = 0; y < wide.height; ++y) lines.push_back(wide.row(y)); strings("wide_frame",lines);
        rows.append(item);
    }));
    EXPECT_EQ(visited,23); EXPECT_EQ(totals.applications,43); EXPECT_EQ(totals.snapshot_mapping,10);
    EXPECT_EQ(totals.unicode,10); EXPECT_EQ(totals.dimensions,10); EXPECT_EQ(totals.foreign_ascii,10);
    EXPECT_EQ(totals.literal_tests,3); EXPECT_EQ(totals.literals.checked,5); EXPECT_EQ(totals.literals.semantic_mapping,3);
    root.set("geometry",drawn); root.set("source_fallback",fallbacks); root.set("geometry_error",geometry_errors);
    root.set("oracle_error",oracle_errors);
    root.set("original_assertion_error_cases",original_errors); root.add("cases",rows); report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto path = std::filesystem::path(LOOM_MERMAID_REPORT_DIR)/"mermaid-termaid-xy23-semantics.json";
    std::ofstream out(path,std::ios::binary);
    ASSERT_TRUE(out && (out << report.to_pretty_string() << '\n')) << "Cannot write " << path;
#endif
}
} // namespace

namespace {
TEST(MermaidChartsXY23Resources, NumericXAdmissionNeverDropsRangeOrExtraSyntax) {
    // Malformed/nonfinite domains and valid numeric profiles not yet represented
    // by this terminal projection preserve the complete source. Such fallback
    // controls do not count as successful drawing cases or remove corpus inputs.
    constexpr std::array<std::string_view,9> sources{{
        "xychart-beta\nx-axis 0 --> 100 --> 200\nline [1,2,3]",
        "xychart-beta\nx-axis 0 -->\nline [1,2,3]",
        "xychart-beta\nx-axis inf --> 100\nline [1,2,3]",
        "xychart-beta\nx-axis -1e308 --> 1e308\nline [1,2,3]",
        "xychart-beta\nx-axis 1 --> 1\nline [1,2,3]",
        "xychart-beta\nx-axis 100 --> 0\nline [1,2,3]",
        "xychart-beta\nx-axis 0 --> 100\nline [1]",
        "xychart-beta horizontal\nx-axis 0 --> 100\nline [1,2,3]",
        "xychart-beta\nx-axis 1 --> 1.0000000000000002\nline [1,2,3]"
    }};
    for (const auto source : sources) { SCOPED_TRACE(std::string(source)); source_fallback(source); }
    const std::string source = "xychart-beta\nx-axis 1 --> 1.0000000000000002\ny-axis 0 --> 4\nline [1,3]";
    const XY expected{"","","",false,{{0,4}},{"1","1.0000000000000002"},{{"line","Line 1",{1,3}}}};
    const auto drawn = capture(loom::ui::render_markdown(fenced(source,"mermaid")),240,256);
    EXPECT_TRUE(xy_errors(drawn,expected).empty()) << explain(xy_errors(drawn,expected));
}
} // namespace

namespace {
TEST(MermaidChartsXY23Oracle, NumericLabelsMatchWholeSignedDecimalExponentTokens) {
    const auto grid = scene({"1 1.0000000000000002 -1 -1.0000000000000002 1e+2 -1e+2 +1 11 0.1 .1 1. 1e-2"});
    auto matches = [&](std::string_view label) { Work work; return find_text(grid,label,work); };
    EXPECT_EQ(matches("1").size(),1u);
    EXPECT_EQ(matches("-1").size(),1u);
    EXPECT_EQ(matches("1.0000000000000002").size(),1u);
    EXPECT_EQ(matches("-1.0000000000000002").size(),1u);
    EXPECT_EQ(matches("1e+2").size(),1u);
    EXPECT_EQ(matches("-1e+2").size(),1u);
    EXPECT_EQ(matches("+1").size(),1u);
    EXPECT_EQ(matches("0.1").size(),1u);
    EXPECT_EQ(matches(".1").size(),1u);
    EXPECT_EQ(matches("1.").size(),1u);
    Work work;
    EXPECT_TRUE(find_text(scene({"1"}),"1.0000000000000002",work).empty());
    EXPECT_TRUE(find_text(scene({"-1"}),"-1.0000000000000002",work).empty());
    EXPECT_TRUE(find_text(scene({"x1 1x _1 1_ +1 -1 .1 1. 1e0 1e-2 1e+2 1+2 1-2"}),"1",work).empty());
    EXPECT_EQ(find_text(scene({"left=1,right:(1)"}),"1",work).size(),2u);
    // The stricter numeric rule must not treat ordinary textual punctuation as
    // part of a number or change the established nonnumeric label boundaries.
    EXPECT_EQ(find_text(scene({"A. A+B A-B"}),"A",work).size(),3u);
}
} // namespace

namespace {
// The source-only facts identify authored domains/configuration. The original
// pinned parser model supplies resolved text and projected nullable samples.
// Neither input depends on the native renderer or repeats a Mermaid parser.
struct MermanAxisPolicy {
    bool labels = true,title = true,ticks = true,line = true;
};
struct MermanPoint {
    double x = 0;
    std::optional<double> value;
};
struct MermanXY {
    XY ownership;
    bool band = true,indexed = false,show_title = true,show_values = false,outside_values = false;
    bool unknown_config = false;
    MermanAxisPolicy x_policy,y_policy;
    double x_first = 0,x_last = 0,y_first = 0,y_last = 0;
    std::optional<std::pair<double,double>> authored_x,authored_y;
    std::vector<std::string> category_names;
    std::vector<std::vector<MermanPoint>> points;
    std::vector<bool> named_series;
};
std::optional<std::pair<double,double>> merman_domain(JsonVal facts,std::string_view axis) {
    const auto value = facts.get("explicit_axis_domains").get(axis);
    if (!value) return {};
    if (!value.is_arr() || value.size() != 2 || !value.at(0).is_num() || !value.at(1).is_num())
        throw std::runtime_error("Invalid frozen authored XY domain");
    const auto a = value.at(0).as_double(),b = value.at(1).as_double();
    if (!std::isfinite(a) || !std::isfinite(b) || a == b || !std::isfinite(b-a))
        throw std::runtime_error("Frozen authored XY domain is nonfinite/degenerate");
    return {{a,b}};
}
MermanXY merman_model(JsonVal model,JsonVal facts) {
    MermanXY result;
    result.ownership.title = model.get_string("title");
    const auto orientation = model.get_string("orientation");
    if (orientation != "vertical" && orientation != "horizontal") throw std::runtime_error("Unknown frozen XY orientation");
    result.ownership.horizontal = orientation == "horizontal";
    const auto x = model.get("xAxis");
    result.band = x.get_string("type") == "band";
    if (!result.band && x.get_string("type") != "linear") throw std::runtime_error("Unknown frozen XY x-axis type");
    result.ownership.category_title = x.get_string("title");
    result.ownership.value_title = model.get("yAxis").get_string("title");
    result.authored_x = merman_domain(facts,"x-axis"); result.authored_y = merman_domain(facts,"y-axis");
    if (result.band) {
        x.get("categories").iter([&](JsonVal value) {
            if (!value.is_str()) throw std::runtime_error("Frozen XY category is not text");
            result.category_names.emplace_back(value.as_str());
        });
        result.indexed = !result.ownership.horizontal && std::ranges::any_of(result.category_names,[](const auto& label) {
            return ftxui::string_width(label) > 12;
        });
    } else {
        result.x_first = x.get("min").as_double(); result.x_last = x.get("max").as_double();
        if (!std::isfinite(result.x_first) || !std::isfinite(result.x_last) || !std::isfinite(result.x_last-result.x_first))
            throw std::runtime_error("Nonfinite frozen numeric XY coordinates");
    }
    int bars = 0,lines = 0; std::size_t samples = 0;
    model.get("plots").iter([&](JsonVal plot) {
        Series owner; owner.type = plot.get_string("type");
        if (owner.type != "bar" && owner.type != "line") throw std::runtime_error("Unknown frozen XY dataset type");
        const auto number = owner.type == "bar" ? ++bars : ++lines;
        owner.name = plot.get_string("title");
        result.named_series.push_back(!owner.name.empty());
        if (owner.name.empty()) owner.name = (owner.type == "bar" ? "Bar " : "Line ") + std::to_string(number);
        plot.get("values").iter([&](JsonVal value) { owner.values.push_back(value.as_double()); });
        std::vector<MermanPoint> points;
        plot.get("data").iter([&](JsonVal datum) {
            if (!datum.is_arr() || datum.size() != 2) throw std::runtime_error("Malformed frozen XY coordinate pair");
            const auto index = points.size(); MermanPoint point;
            if (result.band) {
                if (index >= result.category_names.size() || datum.at(0).as_str() != result.category_names[index])
                    throw std::runtime_error("Frozen XY band coordinate/category mismatch");
                point.x = static_cast<double>(index);
            } else {
                const auto count = plot.get("values").size();
                point.x = count <= 1 ? result.x_first : (index+1 == count ? result.x_last :
                    result.x_first+(result.x_last-result.x_first)*(static_cast<double>(index)/static_cast<double>(count-1)));
                const auto text = datum.at(0).as_str(); double captured = 0;
                const auto parsed = std::from_chars(text.data(),text.data()+text.size(),captured);
                const double tolerance = 16*std::numeric_limits<double>::epsilon()*std::max({1.0,std::abs(captured),std::abs(point.x)});
                if (parsed.ec != std::errc{} || parsed.ptr != text.data()+text.size() || !std::isfinite(captured) ||
                    std::abs(captured-point.x) > tolerance)
                    throw std::runtime_error("Frozen numeric XY sample coordinate differs from declared source transform");
            }
            if (!datum.at(1).is_null()) {
                if (!datum.at(1).is_num() || !std::isfinite(datum.at(1).as_double())) throw std::runtime_error("Nonfinite frozen XY value");
                point.value = datum.at(1).as_double();
                if (index >= owner.values.size() || *point.value != owner.values[index])
                    throw std::runtime_error("Frozen projected XY value mismatch");
                result.y_first = std::min(result.y_first,*point.value); result.y_last = std::max(result.y_last,*point.value);
                ++samples;
            } else if (!result.band || index < owner.values.size()) throw std::runtime_error("Unexpected frozen null XY sample");
            points.push_back(point);
        });
        if (points.size() > 256 || owner.values.size() > points.size()) throw std::runtime_error("Frozen XY sample bound/membership mismatch");
        result.points.push_back(std::move(points)); result.ownership.series.push_back(std::move(owner));
    });
    if (result.ownership.series.size() > 32 || samples > 256 || result.category_names.size() > 256)
        throw std::runtime_error("Frozen XY oracle cardinality bound exceeded");
    if (result.authored_y) { result.y_first = result.authored_y->first; result.y_last = result.authored_y->second; }
    else if (result.y_first == result.y_last) result.y_last = result.y_first+1;
    if (!std::isfinite(result.y_last-result.y_first) || result.y_last == result.y_first)
        throw std::runtime_error("Frozen XY value-axis range is nonfinite/degenerate");
    if (result.authored_x && (result.band || result.x_first != result.authored_x->first || result.x_last != result.authored_x->second))
        throw std::runtime_error("Frozen source/model numeric domain mismatch");
    facts.get("authored_config_fields").iter([&](JsonVal field) {
        std::vector<std::string> path; field.get("path").iter([&](JsonVal key) { path.emplace_back(key.as_str()); });
        if (field.get_string("mapping_category") == "legacy_or_unknown_authored_config_not_present_in_pinned_model_review_required") result.unknown_config = true;
        if (path.empty() || path[0] != "xyChart") return;
        const bool value = field.get("value").as_bool();
        if (path.size() == 2) {
            if (path[1] == "showTitle") result.show_title = value;
            else if (path[1] == "showDataLabel") result.show_values = value;
            else if (path[1] == "showDataLabelOutsideBar") result.outside_values = value;
        } else if (path.size() == 3 && (path[1] == "xAxis" || path[1] == "yAxis")) {
            auto& policy = path[1] == "xAxis" ? result.x_policy : result.y_policy;
            if (path[2] == "showLabel") policy.labels = value;
            else if (path[2] == "showTitle") policy.title = value;
            else if (path[2] == "showTick") policy.ticks = value;
            else if (path[2] == "showAxisLine") policy.line = value;
        }
    });
    return result;
}
template<class Fn> void merman_xy102_native_batch(Fn&& fn) {
    const auto fixtures = std::filesystem::path(LOOM_TESTS_DIR)/"fixtures";
    const auto facts_raw = read(fixtures/"mermaid_native/merman_xy102_facts.json");
    if (digest(facts_raw) != "eeb3c1508b25202838fb175fac8757e4428d2d41e76a0901b6373b04a3f3a3a8")
        throw std::runtime_error("C1-reviewed compact Merman XY facts changed");
    const auto facts = json::parse(facts_raw);
    const auto vendor_root = fixtures/"mermaid_terminal/merman";
    const auto vendor_raw = read(vendor_root/"cases.json");
    const auto vendor = json::parse(vendor_raw);
    if (!facts || !vendor || digest(vendor_raw) != facts->root().get_string("source_case_file_sha256"))
        throw std::runtime_error("Merman native/vendor identity binding failed");
    std::map<std::string,JsonVal> inputs;
    vendor->root().get("cases").iter([&](JsonVal row) {
        if (row.get_string("family") != "xychart") return;
        if (!inputs.emplace(row.get_string("id"),row).second) throw std::runtime_error("Duplicate vendor XY identity");
    });
    std::set<std::string> visited;
    facts->root().get("cases").iter([&](JsonVal fact) {
        const auto id = fact.get_string("id");
        if (!inputs.contains(id) || !visited.insert(id).second) throw std::runtime_error("Native XY identity is absent/duplicate");
        const auto row = inputs.at(id),reference = fact.get("primary_model_reference");
        if (digest(row.get_string("input")) != fact.get_string("input_sha256") ||
            row.get_string("sha256") != fact.get_string("input_sha256")) throw std::runtime_error("Native XY source binding failed");
        const auto original = row.get("expectation").get("upstream_semantic_model");
        if (reference.get_string("path") != original.get_string("path") || reference.get_string("sha256") != original.get_string("sha256"))
            throw std::runtime_error("Native XY primary model selection changed");
        const auto model_raw = read(vendor_root/reference.get_string("path"));
        if (digest(model_raw) != reference.get_string("sha256")) throw std::runtime_error("Native XY model integrity failed");
        const auto model = json::parse(model_raw);
        if (!model) throw std::runtime_error("Cannot parse native XY reference model");
        fn(row,fact,merman_model(model->root(),fact));
    });
    if (visited.size() != 102 || inputs.size() != 102) throw std::runtime_error("Complete native XY identity denominator changed");
}
TEST(MermaidChartsMermanXY102Oracle, SourceOnlyCodecKeepsEveryDomainSlotValueAndConfigIdentity) {
    int sources = 0,nonempty = 0,empty = 0,band = 0,horizontal = 0,multi = 0,indexed = 0,unknown = 0;
    int values = 0,nulls = 0,reversed_x = 0,reversed_y = 0,named = 0,config_fields = 0,tail_values = 0,text_deltas = 0;
    ASSERT_NO_THROW(merman_xy102_native_batch([&](JsonVal row,JsonVal fact,const MermanXY& model) {
        SCOPED_TRACE(row.get_string("id")); ++sources;
        int samples = 0;
        for (const auto& series : model.points) for (const auto& point : series) {
            if (point.value) { ++values; ++samples; } else ++nulls;
        }
        nonempty += samples != 0; empty += samples == 0;
        EXPECT_EQ(fact.get_string("source_obligation"),samples ? "public_nonempty_data_obligation" : "public_empty_state_obligation");
        band += model.band; horizontal += model.ownership.horizontal; multi += model.points.size() > 1;
        indexed += model.indexed; unknown += model.unknown_config;
        reversed_x += model.authored_x && model.authored_x->first > model.authored_x->second;
        reversed_y += model.authored_y && model.authored_y->first > model.authored_y->second;
        named += std::ranges::any_of(model.named_series,[](bool value) { return value; });
        config_fields += static_cast<int>(fact.get("authored_config_fields").size());
        text_deltas += static_cast<int>(fact.get("text_resolution_deltas").size());
        fact.get("source_model_data_transformations").iter([&](JsonVal transform) {
            tail_values += static_cast<int>(transform.get("omitted_authored_tail").size());
        });
    }));
    EXPECT_EQ(sources,102); EXPECT_EQ(nonempty,97); EXPECT_EQ(empty,5); EXPECT_EQ(band,74);
    EXPECT_EQ(horizontal,19); EXPECT_EQ(multi,35); EXPECT_EQ(indexed,2); EXPECT_EQ(unknown,1);
    EXPECT_EQ(values,886); EXPECT_EQ(nulls,3); EXPECT_EQ(reversed_x,2); EXPECT_EQ(reversed_y,3);
    EXPECT_EQ(named,16); EXPECT_EQ(config_fields,160); EXPECT_EQ(tail_values,22); EXPECT_EQ(text_deltas,3);
}
} // namespace

namespace {
struct MermanRectangularProfile {
    TermaidXY view;
    std::string pending;
};
enum class NumericTickProfile { Short, DenseSource };
// Explicit source-model capability. Existing default short-label profiles do
// not execute these scans or acquire new expected draw obligations.
// Dense-only source admission precedes FTXUI temporaries, source-profile deep
// copies and point geometry. Short/default callers never enter this path.
bool dense_source_plain_text(std::string_view text,bool optional,Work& work) {
    if(text.empty()) return optional;
    bool visible=false;
    for(std::size_t at=0;at<text.size();) {
        work.spend();
        const auto lead=static_cast<unsigned char>(text[at++]);
        std::uint32_t scalar=lead;int trailing=0;
        if(lead>=0xc2&&lead<=0xdf) {scalar=lead&0x1f;trailing=1;}
        else if(lead>=0xe0&&lead<=0xef) {scalar=lead&0x0f;trailing=2;}
        else if(lead>=0xf0&&lead<=0xf4) {scalar=lead&0x07;trailing=3;}
        else if(lead>=0x80) return false;
        if(static_cast<std::size_t>(trailing)>text.size()-at) return false;
        for(int n=0;n<trailing;++n) {
            work.spend();const auto next=static_cast<unsigned char>(text[at++]);
            if((next&0xc0)!=0x80) return false;
            scalar=(scalar<<6)|(next&0x3f);
        }
        if((trailing==1&&scalar<0x80)||(trailing==2&&scalar<0x800)||(trailing==3&&scalar<0x10000)||
           scalar>0x10ffff||(scalar>=0xd800&&scalar<=0xdfff)) return false;
        if(scalar<0x20||(scalar>=0x7f&&scalar<=0x9f)||scalar==0x061c||scalar==0x200e||scalar==0x200f||
           scalar==0x2028||scalar==0x2029||(scalar>=0x202a&&scalar<=0x202e)||(scalar>=0x2066&&scalar<=0x2069)) return false;
        visible|=scalar!=' ';
    }
    if(!visible) return false;
    const auto label=std::string(text);
    const auto cells=ftxui::Utf8ToGlyphs(label);
    if(cells.empty()||cells.size()>256||ftxui::string_width(label)>256) return false;
    std::string restored;
    for(const auto& cell:cells) {work.spend(cell.size()+1);restored+=cell;}
    return restored==text;
}
std::vector<std::string> dense_source_copy_admission(const MermanXY& model,std::size_t count,Work& work) {
    const auto& owners=model.ownership.series;
    if(!model.category_names.empty()||!model.ownership.categories.empty())
        return {"Dense numeric source cannot ignore residual categorical fields"};
    if(owners.size()*count>256) return {"Dense numeric source exceeds existing256-sample aggregate bound"};
    for(const auto& owner:owners) if(owner.type!="bar"&&owner.type!="line")
        return {"Dense source owner type must be bar or line before copy"};
    // Four conservative copies cover profile.view.chart, the local expected
    // chart, bounded glyph temporaries and numeric category/owner structures.
    // Check multiplication against remaining Work before doing it.
    auto precharge=[&](std::size_t bytes) {
        if(bytes>work.left/4) throw std::runtime_error("Dense source copy precharge exhausted before text/point processing");
        work.spend(bytes*4);
    };
    precharge(owners.size()+owners.size()*count+count);
    precharge(model.ownership.title.size());precharge(model.ownership.category_title.size());precharge(model.ownership.value_title.size());
    for(const auto& owner:owners) {precharge(owner.type.size());precharge(owner.name.size());}
    if(!dense_source_plain_text(model.ownership.title,true,work)) return {"Dense source title exceeds safe256-cell admission"};
    if(!dense_source_plain_text(model.ownership.category_title,true,work)) return {"Dense source category title exceeds safe256-cell admission"};
    if(!dense_source_plain_text(model.ownership.value_title,true,work)) return {"Dense source value title exceeds safe256-cell admission"};
    for(const auto& owner:owners) if(!dense_source_plain_text(owner.name,false,work))
        return {"Dense source owner name exceeds safe256-cell admission"};
    return {};
}

std::vector<std::string> dense_numeric_model_errors(const MermanXY& model,Work& work) {
    work.spend();
    if (model.band || model.ownership.horizontal || !model.authored_x ||
        model.points.empty() || model.points.size() != model.ownership.series.size() ||
        model.named_series.size() != model.ownership.series.size() || model.points.size() > 32)
        return {"Dense numeric source profile requires vertical explicit numeric x"};
    const double lo = model.x_first,hi = model.x_last,span = hi-lo;
    if (!std::isfinite(lo) || !std::isfinite(hi) || !std::isfinite(span) || !(span > 0) ||
        model.authored_x->first != lo || model.authored_x->second != hi)
        return {"Dense numeric authored x domain invalid"};
    if (!std::isfinite(model.y_first) || !std::isfinite(model.y_last) ||
        !std::isfinite(model.y_last-model.y_first) || !(model.y_last > model.y_first))
        return {"Dense numeric y domain invalid"};
    if (model.authored_y && (!std::isfinite(model.authored_y->first) || !std::isfinite(model.authored_y->second) ||
        !std::isfinite(model.authored_y->second-model.authored_y->first) ||
        model.authored_y->first != model.y_first || model.authored_y->second != model.y_last))
        return {"Dense numeric authored y domain differs from resolved source range"};
    const auto count = model.points.front().size();
    if (count < 3 || count > 64) return {"Dense numeric sample count exceeds independent64-tick verifier profile"};
    const auto copy_errors = dense_source_copy_admission(model,count,work);
    if (!copy_errors.empty()) return copy_errors;
    const double step = span/static_cast<double>(count-1);
    if (!std::isfinite(step) || !(step > 0)) return {"Dense numeric source step invalid"};
    bool long_label = false;
    for (std::size_t owner = 0; owner < model.points.size(); ++owner) {
        work.spend();
        if (model.points[owner].size() != count || model.ownership.series[owner].values.size() != count)
            return {"Dense numeric profile does not erase ragged source samples"};
        double previous = -std::numeric_limits<double>::infinity();
        for (std::size_t n = 0; n < count; ++n) {
            work.spend(65);
            const auto& point = model.points[owner][n]; const double expected = lo+static_cast<double>(n)*step;
            if (!std::isfinite(expected) || !std::isfinite(point.x) || point.x != expected ||
                !(point.x > previous) || !point.value || !std::isfinite(*point.value) ||
                *point.value != model.ownership.series[owner].values[n])
                return {"Dense numeric points contradict the complete uniform source model"};
            previous = point.x;
            const auto text = numeric_x_label(point.x); work.spend(text.size()+1);
            if (!numeric_token(text) || text.size() > 256) return {"Dense numeric coordinate has no complete bounded token"};
            long_label |= ftxui::string_width(text) > 3;
        }
    }
    if (!long_label || model.points.front().front().x != lo || model.points.front().back().x != hi)
        return {"Dense long-token profile requires long labels and exact authored endpoint samples"};
    return {};
}
std::vector<std::string> dense_numeric_roundtrip_errors(const Grid& grid,const TermaidXY& model,const Axes& axis,Work& work) {
    int rows = 0;
    for (int y = axis.y+1; y <= std::min(grid.height-1,axis.y+3); ++y) {
        work.spend(static_cast<std::size_t>(grid.width)); const auto text = grid.row(y);
        if (std::ranges::any_of(text,[](unsigned char c) {
            return c != ' ' && c != '+' && c != '-' && c != '.' && c != 'e' && c != 'E' && (c < '0' || c > '9');
        })) continue;
        work.spend(text.size()*text.size()+1); // Bound repeated token retries on malformed numeric rows.
        const auto tokens = numbers(text); if (tokens.empty()) continue;
        ++rows; if (tokens.size() != model.x_values.size()) return {"Dense numeric token count differs from every source sample"};
        for (std::size_t n = 0; n < tokens.size(); ++n) {
            const auto& token = tokens[n]; work.spend(static_cast<std::size_t>(token.end-token.begin)+1);
            const auto raw = std::string_view(text).substr(static_cast<std::size_t>(token.begin),static_cast<std::size_t>(token.end-token.begin));
            if (token.percent || !numeric_token(raw) || token.value != model.x_values[n])
                return {"Dense numeric full token does not round-trip to its exact source coordinate"};
        }
    }
    return rows == 1 ? std::vector<std::string>{} : std::vector<std::string>{"Dense numeric axis must have one complete tick row"};
}
MermanRectangularProfile merman_rectangular_profile(const MermanXY& model,bool singleton_key = false,
                                                   NumericTickProfile numeric_ticks = NumericTickProfile::Short) {
    MermanRectangularProfile result;
    auto pending = [&](std::string why) { result.pending = std::move(why); return result; };
    if (model.unknown_config) return pending("unknown_plotBorderWidth_profile_unresolved");
    if (model.points.empty()) return pending("empty_state_geometry_profile_pending");
    auto visible = [](const MermanAxisPolicy& axis) { return axis.labels && axis.title && axis.ticks && axis.line; };
    if (!model.show_title || !visible(model.x_policy) || !visible(model.y_policy)) return pending("partial_or_hidden_axis_profile_pending");
    if (model.show_values) return pending("inside_outside_or_terminal_value_label_profile_pending");
    if (model.indexed) return pending("indexed_complete_category_disclosure_profile_pending");
    if (model.y_first > model.y_last || (!model.band && model.x_first > model.x_last)) return pending("directed_reverse_domain_profile_pending");
    const auto& owners = model.ownership.series;
    if (singleton_key) {
        if (owners.size() != 1 || model.named_series.size() != 1 || !model.named_series.front())
            return pending("not_a_source_named_singleton_profile");
        const double span = model.y_last-model.y_first;
        if (!std::isfinite(model.y_first) || !std::isfinite(model.y_last) || !std::isfinite(span) || !(span > 0))
            return pending("named_singleton_invalid_source_domain_or_samples");
        const auto count = owners.front().values.size();
        if (!count || count > 256 || model.points.size() != 1 || model.points.front().size() != count ||
            (model.band && model.category_names.size() != count) ||
            (owners.front().type != "bar" && owners.front().type != "line"))
            return pending("named_singleton_invalid_source_domain_or_samples");
        if (!model.band && (!std::isfinite(model.x_first) || !std::isfinite(model.x_last) ||
            !std::isfinite(model.x_last-model.x_first)))
            return pending("named_singleton_invalid_source_domain_or_samples");
        for (std::size_t n = 0; n < count; ++n) {
            const double value = owners.front().values[n]; const auto& point = model.points.front()[n];
            if (!std::isfinite(value) || value < model.y_first || value > model.y_last || !std::isfinite(point.x) ||
                !point.value || !std::isfinite(*point.value) || *point.value != value)
                return pending("named_singleton_invalid_source_domain_or_samples");
        }
    }
    if (std::ranges::count_if(owners,[](const auto& series) { return series.type == "line"; }) > 2)
        return pending("extended_line_keys_and_overlap_sets_profile_pending");
    for (std::size_t n = 0; n < owners.size(); ++n) {
        if (owners[n].name.find(" + ") != std::string::npos) return pending("ambiguous_composite_owner_name_profile_pending");
        for (std::size_t previous = 0; previous < n; ++previous)
            if (owners[n].name == owners[previous].name) return pending("ambiguous_composite_owner_name_profile_pending");
    }
    if (owners.size() == 1 && model.named_series.front() && !singleton_key) return pending("named_singleton_owner_profile_pending");
    if (model.authored_y && model.y_first != 0 && !(model.y_first > 0 && model.y_first < model.y_last) &&
        std::ranges::any_of(owners,[](const auto& series) { return series.type == "bar"; }))
        return pending("authored_nonzero_domain_bar_baseline_profile_pending");
    const auto count = model.points.front().size();
    if (!count) return pending("empty_dataset_profile_pending");
    if (model.y_first > 0 && model.y_first < model.y_last &&
        std::ranges::all_of(owners,[&](const auto& series) {
            return series.type == "bar" && std::ranges::all_of(series.values,[&](double value) { return value == model.y_first; });
        })) return pending("positive_reference_bar_only_zero_span_outcome_profile_pending");
    for (const auto& points : model.points) {
        if (points.size() != count) return pending("ragged_per_series_numeric_coordinates_profile_pending");
        for (const auto& point : points) {
            if (!point.value) return pending("retained_null_band_slot_profile_pending");
            if (*point.value < model.y_first || *point.value > model.y_last) return pending("owner_marked_boundary_clipping_profile_pending");
        }
    }
    if (!model.band && model.x_first != model.x_last && (model.ownership.horizontal || count < 3))
        return pending("horizontal_or_two_point_numeric_axis_profile_pending");
    if (singleton_key && owners.front().type == "bar") {
        const double reference = model.y_first > 0 ? model.y_first : 0;
        const double capacity = model.ownership.horizontal ? 40 : 20;
        const auto project = [&](double value) { const double unit = (value-model.y_first)/(model.y_last-model.y_first);
            return std::round(capacity*(model.ownership.horizontal ? unit : 1-unit)); };
        for (const double value : owners.front().values) {
            if (value == reference || project(value) != (model.ownership.horizontal ? 0 : 20)) continue;
            const bool negative_endpoint = !model.ownership.horizontal && !model.authored_y && value < 0 &&
                project(value)-project(reference) > 1;
            if (!negative_endpoint) return pending("named_singleton_subcell_category_axis_mark_profile_pending");
        }
    }
    if (singleton_key) {
        // Fixed native capacity bounds the complete table; no map allocation or
        // nonfinite-to-integer conversion precedes source-model validation.
        constexpr int maximum_capacity = 40;
        const int capacity = model.ownership.horizontal ? maximum_capacity : 20;
        std::array<std::optional<double>,maximum_capacity+1> quantized{};
        const double span = model.y_last-model.y_first;
        for (const double value : owners.front().values) {
            const double delta = value-model.y_first,unit = delta/span;
            const double projected = capacity*(model.ownership.horizontal ? unit : 1-unit);
            if (!std::isfinite(delta) || !std::isfinite(unit) || unit < 0 || unit > 1 ||
                !std::isfinite(projected) || projected < 0 || projected > capacity)
                return pending("named_singleton_invalid_source_domain_or_samples");
            const int position = static_cast<int>(std::round(projected));
            auto& entry = quantized[static_cast<std::size_t>(position)];
            if (entry && *entry != value) return pending("named_singleton_precision_disclosure_profile_pending");
            entry = value;
        }
    }
    result.view.chart = model.ownership;
    result.view.chart.singleton_key = singleton_key;
    result.view.chart.single_bar_axis_marker = !model.ownership.horizontal && model.y_first <= 0 && owners.size() == 1 && owners.front().type == "bar";
    result.view.chart.domain = {{model.y_first,model.y_last}};
    if (model.band) result.view.chart.categories = model.category_names;
    else {
        for (const auto& point : model.points.front()) {
            const auto label = numeric_x_label(point.x);
            if (ftxui::string_width(label) > 3 && numeric_ticks == NumericTickProfile::Short)
                return pending("sparse_numeric_tick_profile_pending");
            result.view.chart.categories.push_back(label); result.view.x_values.push_back(point.x);
        }
        if (model.x_first != model.x_last) result.view.x_domain = {{model.x_first,model.x_last}};
    }
    if (result.view.chart.categories.size() != count) return pending("band_sample_count_profile_pending");
    return result;
}
Grid merman_rectangular_compact(const Grid& grid,Work& work) {
    bounded(grid); work.spend(grid.cells.size());
    const auto default_background = ftxui::Color(ftxui::Color::Default).Print(true);
    int height = 1,width = 1;
    for (int y = 0; y < grid.height; ++y) for (int x = 0; x < grid.width; ++x) {
        const auto& cell = grid.at({x,y});
        if ((!cell.text.empty() && cell.text != " ") || (!cell.background.empty() && cell.background != default_background)) {
            height = y+1;
            width = std::max(width,std::min(grid.width,x+std::max(1,ftxui::string_width(cell.text))));
        }
    }
    work.spend(static_cast<std::size_t>(width*height));
    Grid result{width,height,{}}; result.cells.reserve(static_cast<std::size_t>(width*height));
    for (int y = 0; y < height; ++y) result.cells.insert(result.cells.end(),grid.cells.begin()+grid.width*y,grid.cells.begin()+grid.width*y+width);
    return result;
}
void merman_rectangular_source_constraints(const Grid& grid,const XY& expected,const Axes& axis,
                                            Work& work,std::vector<std::string>& errors,const XYValuePlan* value_labels = nullptr) {
    const auto calibrated = scale(grid,axis,expected.horizontal,work);
    if (!calibrated) { errors.push_back("XY source scale is absent"); return; }
    const auto& value_scale = *calibrated;
    const auto first = expected.horizontal ? axis.x : axis.y;
    const auto last = expected.horizontal ? axis.right : axis.top;
    if (value_scale.minimum_position != first || value_scale.maximum_position != last ||
        !expected.domain || value_scale.minimum != expected.domain->first || value_scale.maximum != expected.domain->second)
        errors.push_back("XY numeric source endpoints do not occupy the full physical plot capacity");
    std::vector<std::pair<double,double>> ticks;
    if (!expected.horizontal) {
        for (int y = axis.top; y <= axis.y; ++y) {
            work.spend();
            if (!(stroke(grid.at({axis.x,y}).text).bits & L)) continue;
            std::string prefix; for (int x = 0; x < axis.x; ++x) prefix += grid.at({x,y}).text;
            const auto values = numbers(prefix);
            if (!values.empty() && !values.back().percent && trim(prefix.substr(static_cast<std::size_t>(values.back().end))).empty())
                ticks.emplace_back(values.back().value,y);
        }
    } else {
        const auto line = grid.row(axis.y+1);
        if (!std::ranges::any_of(line,[](unsigned char byte) { return byte >= 128; }))
            for (const auto& number : numbers(line)) {
                work.spend(); const double center = (number.begin+number.end-1)/2.0;
                const int column = static_cast<int>(std::round(center));
                if (!number.percent && column >= axis.x && column <= axis.right && (stroke(grid.at({column,axis.y}).text).bits & D))
                    ticks.emplace_back(number.value,center);
            }
    }
    if (ticks.size() != 5) errors.push_back("XY native visible value axis must preserve all five distinct source-calibrated ticks");
    if (expected.domain) for (int n = 0; n < 5; ++n) {
        const double value = n == 4 ? expected.domain->second : expected.domain->first+(expected.domain->second-expected.domain->first)*(n/4.0);
        const int position = expected.horizontal ? axis.x+n*10 : axis.y-n*5;
        const auto count = std::ranges::count_if(ticks,[&](const auto& tick) { return tick.first == value && tick.second == position; });
        if (count != 1) errors.push_back("XY source value tick missing, duplicated, shifted or rounded to another value");
    }
    const auto ports = anchors(grid,expected,axis,work);
    const auto keys = legend(grid,expected,axis,work,true);
    if (!ports || !keys) { errors.push_back("XY source owner/slot anchors absent"); return; }
    // This native renderer's key rows follow the axis-label row. Chart/axis
    // captions cannot supply a missing primary or composite legend role.
    if (expected.singleton_key) {
        const int row = xy_native_key_start(expected,axis);
        if (keys->primary.size() != 1 || keys->primary.front() != Point{2,row})
            errors.push_back("Named singleton key reused or left its dedicated footer role");
        work.spend(static_cast<std::size_t>(grid.width)); auto text = grid.row(row);
        while (!text.empty() && text.back() == ' ') text.pop_back();
        if (text != grid.at({0,row}).text+" "+expected.series.front().name)
            errors.push_back("Named singleton footer has missing/extra/truncated owner tokens");
    }
    for (const auto at : keys->primary) if (at.y < xy_native_key_start(expected,axis))
        errors.push_back("XY series key reuses a chart/axis caption or axis-label role");
    for (const auto row : keys->composite_rows) if (row < xy_native_key_start(expected,axis))
        errors.push_back("XY composite key reuses a chart/axis caption role");
    for (std::size_t n = 1; n < keys->primary.size(); ++n)
        if (std::pair{keys->primary[n-1].y,keys->primary[n-1].x} >= std::pair{keys->primary[n].y,keys->primary[n].x})
            errors.push_back("XY primary legend rows do not preserve source series order");
    const int bars = static_cast<int>(std::ranges::count_if(expected.series,[](const auto& series) { return series.type == "bar"; }));
    if (!bars) return;
    const double reference = xy_bar_reference(expected);
    const int baseline = static_cast<int>(std::round(value_scale.project(reference)));
    // The new source-derived mode proves the complete exact axis/data union
    // against independently computed midpoint paths in shared_axis_closure.
    if (reference > 0 && !expected.shared_axis) for (const int port : *ports) {
        work.spend(); const Point at = expected.horizontal ? Point{axis.x,port} : Point{port,axis.y};
        const int required = expected.horizontal ? U|D|R : L|R|D;
        if (stroke(grid.at(at).text).bits != required)
            errors.push_back("XY positive-reference category slot lost its axis/tick connections");
    }
    if (expected.single_bar_axis_marker) {
        // A single-owner marker is admitted only at a genuine negative bar end
        // shared with its category-axis downport. It is never a general fill.
        std::set<std::pair<int,int>> endpoints;
        for (std::size_t n = 0; n < expected.series.front().values.size(); ++n) {
            const double value = expected.series.front().values[n];
            const int end = static_cast<int>(std::round(value_scale.project(value)));
            if (value < 0 && end == axis.y && end-baseline > 1) endpoints.emplace((*ports)[n],end);
        }
        for (int y = axis.top; y <= axis.y; ++y) for (int x = axis.x; x <= axis.right; ++x) {
            work.spend();
            if (grid.at({x,y}).text == "⊕" && !endpoints.contains({x,y}))
                errors.push_back("XY shared bar/tick marker is not its source-negative endpoint on the category axis");
        }
    }
    int bar = 0;
    for (std::size_t owner = 0; owner < expected.series.size(); ++owner) {
        const auto& series = expected.series[owner]; if (series.type != "bar") continue;
        work.spend(grid.cells.size()); std::vector<bool> allowed(grid.cells.size());
        const int offset = 2*bar++-(bars-1);
        for (std::size_t n = 0; n < series.values.size(); ++n) {
            const int column = (*ports)[n]+offset;
            const int value = static_cast<int>(std::round(value_scale.project(series.values[n])));
            const int step = value >= baseline ? 1 : -1;
            auto require = [&](int position) {
                work.spend(); const Point at = expected.horizontal ? Point{position,column} : Point{column,position};
                if (!grid.has(at) || (!owns(grid.at(at),owner,expected,*keys,work) && !(value_labels && value_labels->covers(at,owner)))) errors.push_back("XY bar does not occupy its source-fixed owner offset/span");
                if (grid.has(at)) allowed[static_cast<std::size_t>(at.y*grid.width+at.x)] = true;
            };
            if (value == baseline && series.values[n] != reference) {
                if (reference > 0) errors.push_back("XY non-baseline source sample cannot disappear at the positive category axis");
                else require(baseline);
            } else if (value != baseline) for (int position = baseline+step; position != value+step; position += step) require(position);
        }
        for (int y = axis.top; y <= axis.y; ++y) for (int x = axis.x; x <= axis.right; ++x) {
            work.spend(); const Point at{x,y};
            if (owns(grid.at(at),owner,expected,*keys,work) && !allowed[static_cast<std::size_t>(y*grid.width+x)])
                errors.push_back("XY extra/widened/moved bar ink violates the declared one-cell source offset");
        }
    }
}
std::vector<std::string> merman_rectangular_errors(const Grid& original,const MermanXY& model,bool singleton_key = false,
                                                NumericTickProfile numeric_ticks = NumericTickProfile::Short) {
    Work work;
    if (numeric_ticks == NumericTickProfile::DenseSource) {
        auto source_errors = dense_numeric_model_errors(model,work);
        if (!source_errors.empty()) return source_errors;
    }
    const auto profile = merman_rectangular_profile(model,singleton_key,numeric_ticks);
    if (!profile.pending.empty()) return {"Explicit unmet C2 oracle profile: "+profile.pending};
    const auto grid = merman_rectangular_compact(original,work);
    auto expected = profile.view.chart; expected.shared_axis = model.authored_y && shared_axis_source(expected,work);
    std::vector<std::string> best{"XY has no complete fixed-capacity source geometry"};
    int bars = 0,width = 0;
    for (const auto& series : expected.series) bars += series.type == "bar";
    for (const auto& label : expected.categories) width = std::max(width,ftxui::string_width(label));
    const int stride = expected.horizontal ? std::max(4,2*bars+2) : std::max({6,2*bars+3,width+3});
    for (const auto& axis : axes(grid,work)) {
        work.spend(grid.cells.size()); // Account for the candidate Claims zero-initialization.
        auto errors = xy_candidate_errors(grid,expected,axis,work,true);
        if (errors.empty()) {
            const int first = expected.horizontal ? axis.top+2 : axis.x+stride/2;
            const int last = first+static_cast<int>(expected.categories.size()-1)*stride;
            if ((expected.horizontal && (axis.right-axis.x != 40 || axis.y != last+std::max(2,bars))) ||
                (!expected.horizontal && (axis.y-axis.top != 20 || axis.right != last+stride/2)))
                errors.push_back("XY fixed native plot capacity/extent changed");
            const auto ports = anchors(grid,expected,axis,work);
            if (!ports) errors.push_back("XY source coordinate ports missing");
            else for (std::size_t n = 0; n < ports->size(); ++n) {
                work.spend(); if ((*ports)[n] != first+static_cast<int>(n)*stride)
                    errors.push_back("XY category slot independently shifted or compressed");
            }
            if (errors.empty() && numeric_ticks == NumericTickProfile::DenseSource) {
                // The existing lower numeric verifier may reject malformed rows
                // before reaching the exact-roundtrip pass. Charge all three
                // bounded row retries before that potentially failing scan.
                bounded(grid);
                const auto width = static_cast<std::size_t>(grid.width);
                for (int row = axis.y+1; row <= std::min(grid.height-1,axis.y+3); ++row)
                    work.spend(width*width+width+1);
            }
            if (errors.empty()) errors = numeric_x_candidate_errors(grid,profile.view,axis,work);
            if (errors.empty() && numeric_ticks == NumericTickProfile::DenseSource)
                errors = dense_numeric_roundtrip_errors(grid,profile.view,axis,work);
            if (errors.empty()) merman_rectangular_source_constraints(grid,expected,axis,work,errors);
        }
        if (errors.empty()) return {};
        if (best.front() == "XY has no complete fixed-capacity source geometry" || errors.size() < best.size()) best = std::move(errors);
    }
    return best;
}
MermanXY merman_handmade_rectangular() {
    MermanXY model;
    model.ownership = XY{"","","",false,{{0,4}},{},{ {"bar","Bar 1",{2,4}} }};
    model.category_names = {"A","B"}; model.points = {{{0,2},{1,4}}}; model.named_series = {false};
    model.authored_y = {{0,4}}; model.y_last = 4;
    return model;
}
Grid merman_handmade_fixed_bars() {
    Grid grid{15,22,std::vector<Cell>(15u*22u)};
    for (int y = 0; y <= 20; ++y) grid.edit({2,y}).text = "│";
    for (int x = 2; x <= 14; ++x) grid.edit({x,20}).text = "─";
    for (int tick = 0; tick < 5; ++tick) {
        const int y = 20-tick*5; write(grid,{0,y},std::to_string(tick)); grid.edit({2,y}).text = "┤";
    }
    grid.edit({2,20}).text = "┼";
    for (const auto& [column,start,label] : {std::tuple{5,10,"A"},std::tuple{11,0,"B"}}) {
        for (int y = start; y < 20; ++y) grid.edit({column,y}).text = "█";
        grid.edit({column,20}).text = "┬"; write(grid,{column,21},label);
    }
    return grid;
}
TEST(MermaidChartsMermanXY102Oracle, FixedCapacityAndEveryLogicalSlotRejectFittedGeometry) {
    const auto model = merman_handmade_rectangular(); const auto good = merman_handmade_fixed_bars();
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
    auto wrong = good;
    for (int y = 0; y < 20; ++y) { wrong.edit({11,y}).text = " "; wrong.edit({12,y}).text = "█"; }
    wrong.edit({11,20}).text = "─"; wrong.edit({12,20}).text = "┬";
    wrong.edit({11,21}).text = " "; wrong.edit({12,21}).text = "B";
    auto generic = model.ownership; generic.categories = model.category_names;
    EXPECT_TRUE(xy_errors(wrong,generic).empty()) << explain(xy_errors(wrong,generic));
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    // Changing only the data, rather than the category tick/label, must fail.
    wrong = good;
    for (int y = 0; y < 20; ++y) { wrong.edit({11,y}).text = " "; wrong.edit({12,y}).text = "█"; }
    EXPECT_TRUE(xy_errors(wrong,generic).empty()) << explain(xy_errors(wrong,generic));
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good;
    for (int y = 0; y < 20; ++y) wrong.edit({12,y}).text = "█";
    EXPECT_TRUE(xy_errors(wrong,generic).empty()) << explain(xy_errors(wrong,generic));
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    // A long empty axis cannot hide a smaller fitted data capacity.
    wrong = good;
    for (int y = 0; y <= 20; ++y) { wrong.edit({0,y}).text = " "; wrong.edit({2,y}).text = y == 20 ? "┼" : "│"; }
    for (int n = 0; n < 5; ++n) {
        const int y = 20-2*n; write(wrong,{0,y},std::to_string(n)); wrong.edit({2,y}).text = n ? "┤" : "┼";
    }
    for (int y = 0; y < 20; ++y) {
        wrong.edit({5,y}).text = y >= 16 ? "█" : " ";
        wrong.edit({11,y}).text = y >= 12 ? "█" : " ";
    }
    EXPECT_TRUE(xy_errors(wrong,generic).empty()) << explain(xy_errors(wrong,generic));
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good;
    for (const int y : {5,10,15}) { wrong.edit({0,y}).text = " "; wrong.edit({2,y}).text = "│"; }
    EXPECT_TRUE(xy_errors(wrong,generic).empty()) << explain(xy_errors(wrong,generic));
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({0,5}).text = "2";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({11,0}).text = " ";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({7,12}).text = "█";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    EXPECT_FALSE(merman_rectangular_errors(scene({"A 2","B 4"}),model).empty());
    EXPECT_FALSE(merman_rectangular_errors(vertical_bars(),model).empty());
}
MermanXY merman_handmade_named_bars() {
    auto model = merman_handmade_rectangular();
    model.ownership.title = "█ Revenue";
    model.ownership.series = {{"bar","Revenue",{1,3}},{"bar","Forecast",{2,4}}};
    model.points = {{{0,1},{1,3}},{{0,2},{1,4}}}; model.named_series = {true,true};
    return model;
}
Grid merman_handmade_named_geometry() {
    Grid grid{24,26,std::vector<Cell>(24u*26u)};
    write(grid,{0,0},"█ Revenue");
    for (int y = 1; y <= 21; ++y) grid.edit({2,y}).text = "│";
    for (int x = 2; x <= 15; ++x) grid.edit({x,21}).text = "─";
    for (int tick = 0; tick < 5; ++tick) {
        const int y = 21-tick*5; write(grid,{0,y},std::to_string(tick)); grid.edit({2,y}).text = tick ? "┤" : "┼";
    }
    for (const auto& [column,start,swatch] : {std::tuple{4,16,"█"},std::tuple{6,11,"░"},
                                            std::tuple{11,6,"█"},std::tuple{13,1,"░"}})
        for (int y = start; y < 21; ++y) grid.edit({column,y}).text = swatch;
    grid.edit({5,21}).text = "┬"; write(grid,{5,22},"A");
    grid.edit({12,21}).text = "┬"; write(grid,{12,22},"B");
    write(grid,{0,24},"█ Revenue"); write(grid,{0,25},"░ Forecast");
    return grid;
}
TEST(MermaidChartsMermanXY102Oracle, FixedOwnerOffsetsAndFooterRolesCannotBorrowCaptions) {
    const auto model = merman_handmade_named_bars(); const auto good = merman_handmade_named_geometry();
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
    auto wrong = good;
    for (int x = 0; x < wrong.width; ++x) wrong.edit({x,24}).text = " ";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    auto no_title = model; no_title.ownership.title.clear();
    EXPECT_FALSE(merman_rectangular_errors(wrong,no_title).empty());
    auto axis_model = model; axis_model.ownership.title.clear(); axis_model.ownership.category_title = "█ Revenue";
    auto axis_good = good;
    axis_good.cells.resize(static_cast<std::size_t>(axis_good.width*27)); axis_good.height = 27;
    for (int x = 0; x < axis_good.width; ++x) {
        axis_good.edit({x,0}).text = " "; axis_good.edit({x,24}).text = " "; axis_good.edit({x,25}).text = " ";
    }
    write(axis_good,{0,23},axis_model.ownership.category_title);
    write(axis_good,{0,25},"█ Revenue"); write(axis_good,{0,26},"░ Forecast");
    ASSERT_TRUE(merman_rectangular_errors(axis_good,axis_model).empty()) << explain(merman_rectangular_errors(axis_good,axis_model));
    // A footer legend key must not also satisfy a missing authored axis title.
    auto axis_forgery = axis_good;
    for (int x = 0; x < axis_forgery.width; ++x) axis_forgery.edit({x,23}).text = " ";
    EXPECT_FALSE(merman_rectangular_errors(axis_forgery,axis_model).empty());
    // Conversely, the axis-title token cannot replace the separate legend key.
    auto axis_only_key = axis_good;
    for (int x = 0; x < axis_only_key.width; ++x) axis_only_key.edit({x,25}).text = " ";
    EXPECT_FALSE(merman_rectangular_errors(axis_only_key,axis_model).empty());
    wrong = good; wrong.edit({0,24}).text = "░"; wrong.edit({0,25}).text = "█";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good;
    for (int x = 0; x < wrong.width; ++x) std::swap(wrong.edit({x,24}),wrong.edit({x,25}));
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good;
    // Exchange the owner offsets while keeping category ports, keys and values.
    for (int y = 1; y < 21; ++y) {
        std::swap(wrong.edit({4,y}),wrong.edit({6,y}));
        std::swap(wrong.edit({11,y}),wrong.edit({13,y}));
    }
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good;
    // A partial second column is still extra data, even inside the same slot.
    wrong.edit({7,14}).text = "░";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
}
TEST(MermaidChartsMermanXY102Oracle, UnsupportedProfilesRemainExplicitWithoutLosingSources) {
    auto model = merman_handmade_rectangular();
    model.unknown_config = true;
    EXPECT_EQ(merman_rectangular_profile(model).pending,"unknown_plotBorderWidth_profile_unresolved");
    model.unknown_config = false; model.points[0][1].value.reset();
    EXPECT_EQ(merman_rectangular_profile(model).pending,"retained_null_band_slot_profile_pending");
    model = merman_handmade_rectangular(); model.x_policy.labels = false;
    EXPECT_EQ(merman_rectangular_profile(model).pending,"partial_or_hidden_axis_profile_pending");
    model = merman_handmade_rectangular(); model.unknown_config = false; model.show_values = true;
    EXPECT_EQ(merman_rectangular_profile(model).pending,"inside_outside_or_terminal_value_label_profile_pending");
}
bool merman_value_active(const MermanXY& model);
std::vector<std::string> merman_value_errors(const Grid& grid,const MermanXY& model);
std::vector<std::string> xy26_full102_empty_errors(const Grid& grid,JsonVal facts);

// One reviewed source-profile join in this TEST ONLY. The original102 source
// facts/assets stay immutable, and the default Short capability is unchanged.
constexpr std::string_view kD191DenseId="merman/d191355da1e4302cfae5";
constexpr std::string_view kD191DenseSha="d191355da1e4302cfae551e54b8c24ba111076823deb24df68555813bb31d6d7";
constexpr std::string_view kD191DenseSource="xychart\n  x-axis 0 --> 2\n  line [0, 1, 0, 1]\n  bar [1, 0, 1, 0]\n";
constexpr std::string_view kD191ProjectionPath="runtime-capture/oracle-models/c0d45f82c9af375b987646c338cfebce1db6a196f39d54291d4e1f9b9aca6496.json";
constexpr std::string_view kD191ProjectionSha="c0d45f82c9af375b987646c338cfebce1db6a196f39d54291d4e1f9b9aca6496";
void d191_require(bool condition) {
    if(!condition) throw std::runtime_error("Reviewed d191 DenseSource source/Model/profile binding mismatch");
}
void d191_strict_projection(JsonVal projection) {
    // Check relevant types before the existing frozen fact codec. Missing,
    // string or bool numeric fields must not become a default/coerced zero.
    d191_require(projection.is_obj() && projection.get("type").is_str() && projection.get_string("type")=="xychart" &&
        projection.get("orientation").is_str() && projection.get_string("orientation")=="vertical" && projection.get("title").is_null());
    for(const auto axis:{"xAxis","yAxis"}) {
        const auto value=projection.get(axis);const double high=std::string_view(axis)=="xAxis"?2:1;
        d191_require(value.is_obj() && value.get("type").is_str() && value.get_string("type")=="linear" &&
            value.get("title").is_str() && value.get_string("title").empty() && value.get("min").is_num() &&
            value.get("max").is_num() && value.get("min").as_double()==0 && value.get("max").as_double()==high);
    }
    const auto plots=projection.get("plots");d191_require(plots.is_arr() && plots.size()==2);
    constexpr std::array<std::string_view,4> coordinates{{"0","0.6666666666666666","1.3333333333333333","2"}};
    for(std::size_t owner=0;owner<2;++owner) {
        const auto plot=plots.at(owner);const auto values=plot.get("values"),data=plot.get("data");
        d191_require(plot.is_obj() && plot.get("type").is_str() && plot.get_string("type")== (owner?"bar":"line") &&
            plot.get("title").is_str() && plot.get_string("title").empty() && values.is_arr() && values.size()==4 && data.is_arr() && data.size()==4);
        for(std::size_t n=0;n<4;++n) {
            const double expected=static_cast<double>((n+owner)%2);const auto point=data.at(n);
            d191_require(values.at(n).is_num() && values.at(n).as_double()==expected && point.is_arr() && point.size()==2 &&
                point.at(0).is_str() && point.at(0).as_str()==coordinates[n] && point.at(1).is_num() && point.at(1).as_double()==expected);
        }
    }
}
void d191_dense_binding(JsonVal row,JsonVal fact,const MermanXY& model,NumericTickProfile profile) {
    d191_require(profile==NumericTickProfile::DenseSource && row.get("id").is_str() && row.get_string("id")==kD191DenseId &&
        row.get("family").is_str() && row.get_string("family")=="xychart" &&
        row.get("sha256").is_str() && row.get_string("sha256")==kD191DenseSha && row.get("input").is_str() &&
        row.get("input").as_str()==kD191DenseSource && digest(row.get("input").as_str())==kD191DenseSha &&
        fact.get("id").is_str() && fact.get_string("id")==kD191DenseId && fact.get("input_sha256").is_str() &&
        fact.get_string("input_sha256")==kD191DenseSha && fact.get_string("source_obligation")=="public_nonempty_data_obligation" &&
        fact.get("typed_facts_canonical_sha256").is_str() && fact.get_string("typed_facts_canonical_sha256")==
        "1d5294e5cb5a99c8de9253cabda0aa6f24b35a59f3e2efae46735339baaf7b8b" &&
        fact.get("post_parse_model_test_isolation").is_null());
    const auto reference=fact.get("primary_model_reference"),domains=fact.get("explicit_axis_domains"),x=domains.get("x-axis");
    d191_require(reference.get("path").is_str() && reference.get_string("path")==kD191ProjectionPath &&
        reference.get("sha256").is_str() && reference.get_string("sha256")==kD191ProjectionSha && reference.get("bytes").is_int() &&
        reference.get_int("bytes")==24093 && domains.is_obj() && !domains.has("y-axis") && x.is_arr() && x.size()==2 &&
        x.at(0).is_num() && x.at(1).is_num() && x.at(0).as_double()==0 && x.at(1).as_double()==2);
    for(const auto field:{"authored_config_fields","band_missing_sample_slots","source_model_data_transformations","text_resolution_deltas","projected_values_outside_authored_y"})
        d191_require(fact.get(field).is_arr() && fact.get(field).size()==0);
    const auto bytes=read(std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_terminal/merman"/kD191ProjectionPath);
    d191_require(bytes.size()==24093 && digest(bytes)==kD191ProjectionSha);const auto projection=json::parse(bytes);
    d191_require(static_cast<bool>(projection));d191_strict_projection(projection->root());
    // The full native Model must still be this complete reviewed source, not
    // merely some arbitrary model accepted by the general DenseSource profile.
    auto visible=[](const MermanAxisPolicy& axis){return axis.labels&&axis.title&&axis.ticks&&axis.line;};
    d191_require(!model.band && !model.indexed && !model.ownership.horizontal && model.show_title && !model.show_values &&
        !model.outside_values && !model.unknown_config && visible(model.x_policy) && visible(model.y_policy) &&
        model.authored_x && model.authored_x->first==0 && model.authored_x->second==2 && !model.authored_y &&
        model.x_first==0 && model.x_last==2 && model.y_first==0 && model.y_last==1 &&
        model.ownership.title.empty() && model.ownership.category_title.empty() && model.ownership.value_title.empty() &&
        !model.ownership.domain && !model.ownership.single_bar_axis_marker && !model.ownership.shared_axis &&
        !model.ownership.singleton_key && model.ownership.categories.empty() && model.category_names.empty() &&
        model.ownership.series.size()==2 && model.points.size()==2 && model.named_series.size()==2 &&
        !model.named_series[0] && !model.named_series[1]);
    const double step=2.0/3.0;
    for(std::size_t owner=0;owner<2;++owner) {
        const auto& series=model.ownership.series[owner];
        d191_require(series.type==(owner?"bar":"line") && series.name==(owner?"Bar 1":"Line 1") &&
            series.values.size()==4 && model.points[owner].size()==4);
        for(std::size_t n=0;n<4;++n) {
            const double value=static_cast<double>((n+owner)%2);const auto& point=model.points[owner][n];
            d191_require(series.values[n]==value && point.value && *point.value==value && point.x==static_cast<double>(n)*step);
        }
    }
}

TEST(MermaidChartsSemantics, CompleteMermanXY102Batch) {
    json::JsonMutDoc report; auto root = report.object(); auto rows = root.make_arr();
    root.set("kind","complete_merman_xy102_C2_source_profile_geometry_baseline");
    root.set("qualification","Every102 source executes. Reviewed rectangular and value-label source profiles require real geometry; other C2 profiles remain explicit unmet obligations. Candidate art is never an automatic pass. Original foreign/API predicates remain separate.");
    root.set("expected_nonempty_geometry",97); root.set("expected_empty_states",5);
    int visited = 0,candidates = 0,fallbacks = 0,geometry_matches = 0,pending = 0,geometry_errors = 0,oracle_errors = 0;
    int candidate_pending = 0,source_pending = 0,candidate_active = 0,source_active = 0;
    int empty_matches = 0,empty_state_errors = 0;
    ASSERT_NO_THROW(merman_xy102_native_batch([&](JsonVal row,JsonVal facts, const MermanXY& model) {
        ++visited; const auto id = row.get_string("id"),source = row.get_string("input"); SCOPED_TRACE(id);
        const bool dense_profile = id == kD191DenseId;
        const auto numeric_ticks = dense_profile ? NumericTickProfile::DenseSource : NumericTickProfile::Short;
        if(dense_profile) d191_dense_binding(row,facts,model,numeric_ticks);
        const auto profile = merman_rectangular_profile(model,false,numeric_ticks);
        const bool value_profile = merman_value_active(model);
        const bool named_profile = model.ownership.series.size() == 1 && model.named_series.size() == 1 && model.named_series.front() &&
            merman_rectangular_profile(model,true).pending.empty();
        const bool empty_profile = model.ownership.series.empty() && facts.get_string("source_obligation") == "public_empty_state_obligation";
        const bool audited_states = value_profile || named_profile || empty_profile || dense_profile,active = audited_states || profile.pending.empty();
        auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
        auto ordinary = loom::ui::render_markdown(fenced(source,"text"));
        const auto wide = capture(element,240,256);
        const bool is_source = same_source_surface(wide,capture(ordinary,240,256));
        if (is_source) ++fallbacks; else ++candidates;
        if (active) { if (is_source) ++source_active; else ++candidate_active; }
        else { if (is_source) ++source_pending; else ++candidate_pending; }
        std::vector<std::string> errors; bool oracle_error = false;
        if (!active) { ++pending; errors.push_back("Explicit unmet C2 oracle profile: "+profile.pending); }
        else if (is_source) errors.push_back("Complete authored XY source retained as fallback");
        else {
            try { errors = empty_profile ? xy26_full102_empty_errors(wide,facts) :
                (value_profile ? merman_value_errors(wide,model) : merman_rectangular_errors(wide,model,named_profile,numeric_ticks)); }
            catch (const std::exception& error) { oracle_error = true; errors.push_back("Explicit XY oracle error: "+std::string(error.what())); }
            if (!audited_states) {
                if (oracle_error) ++oracle_errors;
                else if (errors.empty()) ++geometry_matches;
                else ++geometry_errors;
            }
        }
        if (!audited_states) EXPECT_TRUE(errors.empty()) << explain(errors);
        const auto narrow = capture(element,40,256);
        const bool narrow_source = same_source_surface(narrow,capture(ordinary,40,256));
        std::vector<std::string> narrow_errors;
        if (active && !is_source && errors.empty() && !narrow_source) {
            try { narrow_errors = empty_profile ? xy26_full102_empty_errors(narrow,facts) :
                (value_profile ? merman_value_errors(narrow,model) : merman_rectangular_errors(narrow,model,named_profile,numeric_ticks)); }
            catch (const std::exception& error) {
                narrow_errors.push_back("Explicit narrow XY oracle error: "+std::string(error.what()));
                if (audited_states) oracle_error = true;
            }
            EXPECT_TRUE(narrow_errors.empty()) << explain(narrow_errors);
        }
        if (empty_profile && narrow_source) narrow_errors.emplace_back("Reviewed complete empty profile fits40 but remains source");
        const auto restored = capture(element,240,256);
        EXPECT_TRUE(restored == wide);
        EXPECT_NE(wide.text().find(kSentinel),std::string::npos);
        EXPECT_NE(narrow.text().find(kSentinel),std::string::npos);
        if (audited_states) {
            for (const auto& error : narrow_errors) errors.push_back(std::string(dense_profile ? "Dense numeric source narrow geometry: " : (empty_profile ? "Empty profile narrow roles: " : (named_profile ? "Named profile narrow geometry: " : "Value profile narrow geometry: ")))+error);
            if (!(restored == wide)) errors.push_back(dense_profile ? "Dense numeric source changed the complete restored surface" : (empty_profile ? "Empty profile changed the complete restored surface" : (named_profile ? "Named profile changed the complete restored surface" : "Value profile changed the complete restored surface")));
            if (wide.text().find(kSentinel) == std::string::npos || narrow.text().find(kSentinel) == std::string::npos)
                errors.push_back(dense_profile ? "Dense numeric source lost following content" : (empty_profile ? "Empty profile lost following content" : (named_profile ? "Named profile lost following content" : "Value profile lost following content")));
            if (!is_source) {
                if (oracle_error) ++oracle_errors;
                else if (errors.empty()) { if (empty_profile) ++empty_matches; else ++geometry_matches; }
                else { if (empty_profile) ++empty_state_errors; else ++geometry_errors; }
            }
            EXPECT_TRUE(errors.empty()) << explain(errors);
        }
        auto item = root.make_obj(); item.set("id",id); item.set("input_sha256",digest(source));
        item.set("narrow_observed",narrow_source ? "source_fallback" : "candidate_art");
        item.set("narrow_geometry_errors",static_cast<int>(narrow_errors.size()));
        item.set("observed",is_source ? "source_fallback" : "candidate_art");
        item.set("oracle_profile",dense_profile ? "reviewed_dense_numeric_source_coordinates" : (empty_profile ? "complete_empty_source_fields" : (value_profile ? "value_labels_source_coordinates" : (named_profile ? "named_singleton_source_coordinates" : (profile.pending.empty() ? "rectangular_source_coordinates" : profile.pending)))));
        item.set("geometry_outcome",!active ? "oracle_profile_pending" :
            (is_source ? (empty_profile ? "empty_state_source_unmet" : "source_fallback") : (oracle_error ? "oracle_error" :
                (errors.empty() ? (empty_profile ? "empty_state_oracle_match_pending_review" : "geometry_oracle_match_pending_review") :
                    (empty_profile ? "empty_state_error" : "geometry_error")))));
        item.set("restored_identically",restored == wide);
        item.set("original_assertion_applications_pending",static_cast<int>(row.get("expectation").get("assertions").size()));
        auto error_array = root.make_arr(); for (const auto& error : errors) error_array.append(root.make_str(error)); item.add("errors",error_array);
        auto frame = root.make_arr(); for (int y = 0; y < wide.height; ++y) frame.append(root.make_str(wide.row(y))); item.add("wide_frame",frame);
        rows.append(item);
    }));
    EXPECT_EQ(visited,102); EXPECT_EQ(candidates+fallbacks,102);
    EXPECT_EQ(candidate_pending+source_pending+candidate_active+source_active,102);
    EXPECT_EQ(candidate_active,geometry_matches+empty_matches+geometry_errors+empty_state_errors+oracle_errors);
    root.set("exact_inputs",visited); root.set("candidate_art",candidates); root.set("source_fallback",fallbacks);
    auto partition = root.make_obj(); partition.set("candidate_art_profile_pending",candidate_pending);
    partition.set("source_fallback_profile_pending",source_pending); partition.set("candidate_art_active_oracle",candidate_active);
    partition.set("source_fallback_active_oracle",source_active); root.add("disjoint_observation_profile_partition",partition);
    root.set("geometry_oracle_matches_pending_review",geometry_matches); root.set("oracle_profile_pending",pending);
    root.set("geometry_error",geometry_errors); root.set("oracle_error",oracle_errors);
    root.set("empty_state_matches_pending_review",empty_matches); root.set("empty_state_errors",empty_state_errors);
    root.set("certified_complete_geometry",0); root.set("certified_empty_states",0);
    root.set("original_function_case_applications_pending",108); root.add("cases",rows); report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto path = std::filesystem::path(LOOM_MERMAID_REPORT_DIR)/"mermaid-merman-xy102-C2-semantics.json";
    std::ofstream out(path,std::ios::binary);
    ASSERT_TRUE(out && (out << report.to_pretty_string() << '\n')) << "Cannot write " << path;
#endif
}
} // namespace

namespace {
TEST(MermaidChartsMermanXY102Resources, DefaultNumericCoordinatesSignedDecimalsAndAxisTitlesRemainGeometric) {
    MermanXY model;
    model.band = false; model.x_first = 1; model.x_last = 3; model.y_first = -1; model.y_last = 1;
    model.ownership = XY{"","X","Y",false,{{-1,1}},{},{{"bar","Bar 1",{1,0.5,-1}}}};
    model.points = {{{1,1},{2,0.5},{3,-1}}}; model.named_series = {false};
    const std::string source = "xychart\nx-axis X\ny-axis \"  Y   \"\nbar [+1, .5, -1]\n";
    auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
    const auto wide = capture(element,240,256);
    EXPECT_TRUE(merman_rectangular_errors(wide,model).empty()) << explain(merman_rectangular_errors(wide,model));
    EXPECT_FALSE(same_source_surface(wide,capture(loom::ui::render_markdown(fenced(source,"text")),240,256)));
    static_cast<void>(capture(element,40,256));
    EXPECT_TRUE(capture(element,240,256) == wide);
    const auto named = merman_handmade_named_bars();
    const std::string named_source = "xychart\ntitle \"█ Revenue\"\nx-axis [A, B]\ny-axis 0 --> 4\nbar \"Revenue\" [1,3]\nbar \"Forecast\" [2,4]\n";
    const auto named_frame = capture(loom::ui::render_markdown(fenced(named_source,"mermaid")),240,256);
    EXPECT_TRUE(merman_rectangular_errors(named_frame,named).empty()) << explain(merman_rectangular_errors(named_frame,named));
    for (const auto value : {"+ 1","+ -1","++1","--1","1.","1e3"}) {
        SCOPED_TRACE(value);
        source_fallback("xychart\nline ["+std::string(value)+",2,3]\n");
    }
    for (const auto axis : {"x-axis","y-axis","x-axis \"X\" trailing","y-axis 0 --> 4 trailing"})
        source_fallback("xychart\n"+std::string(axis)+"\nline [1,2,3]\n");
    // The exact named-line source is now checked by the named-source gate
    // below; the malformed numeric/axis/pie controls remain source fallbacks.
    source_fallback("pie\n\"A\" : +1\n");
}
TEST(MermaidChartsMermanXY102Resources, ReviewedMetadataMapsToNativeMetricsAndUnknownOptionsKeepWholeSource) {
    const auto model = merman_handmade_rectangular();
    const std::string body = "xychart\nx-axis [A, B]\ny-axis 0 --> 4\nbar [2,4]\n";
    const std::string metadata = "---\nconfig:\n  theme: dark\n  xyChart:\n    width: 200\n    height: 20\n    plotReservedSpacePercent: 100\n  themeVariables:\n    xyChart:\n      backgroundColor: '#f0f8ff'\n      plotColorPalette: '#008000, #faba63'\n---\n";
    const auto plain = capture(loom::ui::render_markdown(fenced(body,"mermaid")),240,256);
    const auto configured = capture(loom::ui::render_markdown(fenced(metadata+body,"mermaid")),240,256);
    EXPECT_TRUE(merman_rectangular_errors(configured,model).empty()) << explain(merman_rectangular_errors(configured,model));
    EXPECT_TRUE(configured == plain);
    // This exact source used to be an unmet-label fallback control. It now
    // retains the same 0..4 domain, A/B slots and 2/4 values as real geometry.
    const std::string labels = "---\nconfig:\n  xyChart:\n    showDataLabel: true\n---\n"+body;
    auto value_model = model; value_model.show_values = true;
    auto value_element = loom::ui::render_markdown(fenced(labels,"mermaid"));
    auto value_plain = loom::ui::render_markdown(fenced(labels,"text"));
    const auto value_wide = capture(value_element,240,256);
    EXPECT_FALSE(same_source_surface(value_wide,capture(value_plain,240,256)));
    for (const int width : {240,40,240}) {
        const auto frame = capture(value_element,width,256);
        if (width == 240 || !same_source_surface(frame,capture(value_plain,width,256))) {
            const auto errors = merman_value_errors(frame,value_model); EXPECT_TRUE(errors.empty()) << explain(errors);
        }
        if (width == 240) EXPECT_TRUE(frame == value_wide);
        EXPECT_NE(frame.text().find(kSentinel),std::string::npos);
    }
    for (const auto options : {
        "  xyChart:\n    plotBorderWidth: 5\n",
        "",
        "  xyChart:\n",
        "  xyChart:\n    xAxis:\n",
        "  theme: dark\n%% invalid YAML statement\n",
        "  xyChart:\n    showTitle: false\n",
        "  xyChart:\n    xAxis:\n      showLabel: false\n",
        "  xyChart:\n    width: 200\n    width: 300\n",
        "  xyChart:\n    width: '200'\n",
        "  xyChart:\n    width: 200\n     height: 20\n",
        "  xyChart:\n    width: &metric 200\n",
        "  theme: imaginary\n",
        "  theme: dark\n   xyChart:\n    width: 200\n",
        "  themeVariables:\n    xyChart:\n      backgroundColor: #f0f8ff\n",
        "  themeVariables:\n    xyChart:\n      plotColorPalette: '#008000,'\n",
        "  unknown:\n    width: 200\n",
        "\txyChart:\n\t  width: 200\n"}) {
        SCOPED_TRACE(options);
        source_fallback("---\nconfig:\n"+std::string(options)+"---\n"+body);
    }
    source_fallback(metadata+body+"unparsed trailing statement\n");
}
} // namespace

namespace {
TEST(MermaidChartsMermanXY102Resources, UnquotedTokenConcatenationAndQuotedWhitespaceHaveDifferentSourceMeaning) {
    auto model = merman_handmade_rectangular(); model.ownership.series[0].values = {1,2};
    model.points = {{{0,1},{1,2}}};
    model.category_names = {"cat2asdf","B"};
    const auto joined = capture(loom::ui::render_markdown(fenced("xychart\nx-axis [cat2 asdf,B]\ny-axis 0 --> 4\nbar [1,2]\n","mermaid")),240,256);
    EXPECT_TRUE(merman_rectangular_errors(joined,model).empty()) << explain(merman_rectangular_errors(joined,model));
    model.category_names = {"cat2  asdf","B"};
    const auto quoted = capture(loom::ui::render_markdown(fenced("xychart\nx-axis [\"cat2  asdf\",B]\ny-axis 0 --> 4\nbar [1,2]\n","mermaid")),240,256);
    EXPECT_TRUE(merman_rectangular_errors(quoted,model).empty()) << explain(merman_rectangular_errors(quoted,model));
    EXPECT_FALSE(joined == quoted);
    for (const auto token : {"cat@x","cat/x","cat:x","cat!x"}) {
        SCOPED_TRACE(token);
        source_fallback("xychart\nx-axis ["+std::string(token)+",B]\ny-axis 0 --> 4\nbar [1,2]\n");
    }
}
TEST(MermaidChartsOfficialXY, AuthoredWideNumericLegendHasCompleteIndependentGeometry) {
    const auto path = std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_official/cases/xychart/nightly-check/should-keep-legend-inside-a-wide-chart-scaled-to-fit.mmd";
    const auto source = read(path);
    ASSERT_EQ(digest(source),"c6c68096d1acd12f38c5d8c402e2223bd475a716a25c1a36b87b061825a4f0a6");
    // Hand-read authored facts. Source width:4000 is a reviewed SVG metric;
    // this gate proves native coordinates and owners, not the SVG width rule.
    TermaidXY model;
    model.chart = XY{"Wide chart scaled down to fit","Elapsed Time","Percentage (%)",false,{{0,100}},{},
        {{"line","CPU",{10,20,30,40,50,60,70,80,90,100}},
         {"line","RAM",{20,30,40,50,60,70,80,90,95,90}}}};
    model.x_domain = {{0,10}};
    for (int n = 0; n < 10; ++n) {
        const double value = n == 9 ? 10 : (10.0/9.0)*n;
        model.x_values.push_back(value); model.chart.categories.push_back(numeric_x_label(value));
    }
    auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
    const auto wide = capture(element,240,256);
    const auto errors = termaid_xy_errors(wide,model);
    EXPECT_TRUE(errors.empty()) << explain(errors);
    EXPECT_FALSE(same_source_surface(wide,capture(loom::ui::render_markdown(fenced(source,"text")),240,256)));
    const auto narrow = capture(element,40,256);
    EXPECT_TRUE(same_source_surface(narrow,capture(loom::ui::render_markdown(fenced(source,"text")),40,256))) << "Complete numeric labels need whole-source narrow fallback";
    EXPECT_TRUE(capture(element,240,256) == wide);
    auto missing = model; missing.chart.series[0].values[4] = 99;
    EXPECT_FALSE(termaid_xy_errors(wide,missing).empty());
    missing = model; std::swap(missing.chart.series[0].name,missing.chart.series[1].name);
    EXPECT_FALSE(termaid_xy_errors(wide,missing).empty());
}
} // namespace

namespace {
TEST(MermaidChartsMermanXY102Oracle, NegativeBarEndpointSharesOnlyItsRealCategoryAxisTick) {
    auto model = merman_handmade_rectangular(); model.authored_y.reset();
    model.y_first = -4; model.y_last = 4; model.ownership.domain = {{-4,4}};
    model.ownership.series[0].values = {-4,4}; model.points = {{{0,-4},{1,4}}};
    Grid good{16,22,std::vector<Cell>(16u*22u)};
    for (int y = 0; y <= 20; ++y) good.edit({3,y}).text = "│";
    for (int x = 3; x <= 15; ++x) good.edit({x,20}).text = "─";
    for (int n = 0; n < 5; ++n) {
        const auto text = std::to_string(-4+2*n); const int y = 20-5*n;
        write(good,{2-static_cast<int>(text.size()),y},text); good.edit({3,y}).text = n ? "┤" : "┼";
    }
    for (int y = 11; y < 20; ++y) good.edit({6,y}).text = "█";
    good.edit({6,20}).text = "⊕";
    for (int y = 0; y < 10; ++y) good.edit({12,y}).text = "█";
    good.edit({12,20}).text = "┬"; write(good,{6,21},"A"); write(good,{12,21},"B");
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
    auto wrong = good; wrong.edit({6,20}).text = "█";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({6,20}).text = "━";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({5,20}).text = "│";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({7,20}).text = "│";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({6,15}).text = "⊕";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({9,20}).text = "⊕";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; for (int y = 11; y < 20; ++y) wrong.edit({6,y}).text = " ";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    auto less = model; less.ownership.series[0].values[0] = -2; less.points[0][0].value = -2;
    EXPECT_FALSE(merman_rectangular_errors(good,less).empty());
}
} // namespace

namespace {
TEST(MermaidChartsMermanXY102Oracle, SourceSurfaceIgnoresOnlyLanguageForegroundAndNeverLostGlyphsOrCanvas) {
    const auto source = scene({"xychart","bar [1,2]"});
    auto changed = source;
    for (auto& cell : changed.cells) cell.foreground = "different language token color";
    EXPECT_FALSE(changed == source);
    EXPECT_TRUE(same_source_surface(changed,source));
    changed.edit({0,1}).text = " ";
    EXPECT_FALSE(same_source_surface(changed,source));
    changed = source; changed.width -= 1;
    EXPECT_FALSE(same_source_surface(changed,source));
    changed = source; changed.edit({0,1}).background = "different canvas";
    EXPECT_FALSE(same_source_surface(changed,source));
    // Preserve the original valid source. Its formerly unsupported label
    // option now requires the exact inferred 0..2 model and real plotted bars.
    const std::string raw = "---\nconfig:\n  xyChart:\n    showDataLabel: true\n---\nxychart\nx-axis [A,B]\nbar [1,2]\n";
    auto model = merman_handmade_rectangular(); model.show_values = true;
    model.authored_y.reset(); model.y_last = 2; model.ownership.domain = {{0,2}};
    model.ownership.series[0].values = {1,2}; model.points = {{{0,1},{1,2}}};
    auto element = loom::ui::render_markdown(fenced(raw,"mermaid"));
    auto ordinary = loom::ui::render_markdown(fenced(raw,"text"));
    const auto wide = capture(element,240,256);
    EXPECT_FALSE(same_source_surface(wide,capture(ordinary,240,256)));
    for (const int width : {240,40,240}) {
        const auto frame = capture(element,width,256);
        if (width == 240 || !same_source_surface(frame,capture(ordinary,width,256))) {
            const auto errors = merman_value_errors(frame,model); EXPECT_TRUE(errors.empty()) << explain(errors);
        }
        if (width == 240) EXPECT_TRUE(frame == wide);
        EXPECT_NE(frame.text().find(kSentinel),std::string::npos);
    }
    // An unknown option retains a real true-token source-color witness. Only
    // language foreground may differ; every glyph and background must match.
    const std::string unsupported = "---\nconfig:\n  xyChart:\n    showDataLabel: true\n    unknown: true\n---\nxychart\nx-axis [A,B]\nbar [1,2]\n";
    auto fallback = loom::ui::render_markdown(fenced(unsupported,"mermaid"));
    auto fallback_plain = loom::ui::render_markdown(fenced(unsupported,"text"));
    const auto fallback_wide = capture(fallback,240,256);
    EXPECT_FALSE(fallback_wide == capture(fallback_plain,240,256));
    for (const int width : {240,40,240}) {
        const auto frame = capture(fallback,width,256);
        EXPECT_TRUE(same_source_surface(frame,capture(fallback_plain,width,256)));
        if (width == 240) EXPECT_TRUE(frame == fallback_wide);
        EXPECT_NE(frame.text().find(kSentinel),std::string::npos);
    }
}
} // namespace

namespace {
TEST(MermaidChartsMermanXY102Resources, UnreviewedCategoryAxisIntersectionsRetainTheCompleteSource) {
    for (const std::string source : {
        "xychart horizontal\nx-axis [A,B]\nbar [-4,4]\n",
        "xychart\nx-axis [A,B]\nbar [0.00001,4]\n",
        "xychart horizontal\nx-axis [A,B]\nbar [0.00001,4]\n",
        "xychart\nx-axis [A,B]\ny-axis -4 --> 4\nbar [-4,4]\n",
        "xychart\nx-axis [A,B]\nbar [-4,4]\nbar [2,4]\n",
        "xychart\nx-axis [A,B]\nbar [-1,100000]\n"}) {
        SCOPED_TRACE(source); source_fallback(source);
    }
    auto model = merman_handmade_rectangular(); model.authored_y.reset();
    model.y_first = -4; model.y_last = 4;
    model.ownership.series[0].values = {-4,4}; model.points = {{{0,-4},{1,4}}};
    const std::string reviewed = "xychart\nx-axis [A,B]\nbar [-4,4]\n";
    const auto drawn = capture(loom::ui::render_markdown(fenced(reviewed,"mermaid")),240,256);
    EXPECT_TRUE(merman_rectangular_errors(drawn,model).empty()) << explain(merman_rectangular_errors(drawn,model));
    model.y_first = 0; model.ownership.series[0].values = {0,4}; model.points = {{{0,0},{1,4}}};
    const auto zero = capture(loom::ui::render_markdown(fenced("xychart\nx-axis [A,B]\nbar [0,4]\n","mermaid")),240,256);
    EXPECT_TRUE(merman_rectangular_errors(zero,model).empty()) << explain(merman_rectangular_errors(zero,model));
}
} // namespace

namespace {
MermanXY merman_handmade_composite_model(bool coincident = false) {
    auto model = merman_handmade_rectangular();
    model.ownership.series = {{"bar","Bar 1",{2,4}},{"bar","Bar 2",{3,1}},
                              {"line","Line 1",{3,3}},{"line","Line 2",coincident ? std::vector<double>{3,3} : std::vector<double>{1,1}}};
    model.points = {{{0,2},{1,4}},{{0,3},{1,1}},{{0,3},{1,3}},
                    coincident ? std::vector<MermanPoint>{{0,3},{1,3}} : std::vector<MermanPoint>{{0,1},{1,1}}};
    model.named_series = {false,false,false,false};
    return model;
}
void merman_handmade_composite_key(Grid& g,int row,std::string_view text) {
    for (int x = 0; x < g.width; ++x) g.edit({x,row}).text = " ";
    write(g,{0,row},text);
}
Grid merman_handmade_composite_scene(bool coincident = false) {
    // Legend capacity is independent of the fixed plot extent x2..15. The
    // longest malformed-owner mutation must also fit without row overflow.
    Grid g{48,32,std::vector<Cell>(48u*32u)};
    for (int y = 0; y <= 20; ++y) g.edit({2,y}).text = "│";
    for (int x = 2; x <= 15; ++x) g.edit({x,20}).text = "─";
    for (int n = 0; n < 5; ++n) {
        write(g,{0,20-5*n},std::to_string(n)); g.edit({2,20-5*n}).text = n ? "┤" : "┼";
    }
    // Source-fixed Bar1 offsets -1 and Bar2 offsets +1 at A5/B12.
    for (const auto& [column,first,ink] : {std::tuple{4,10,"█"},std::tuple{11,0,"█"},
                                        std::tuple{6,5,"░"},std::tuple{13,15,"░"}})
        for (int y = first; y < 20; ++y) g.edit({column,y}).text = ink;
    g.edit({5,20}).text = "┬"; write(g,{5,21},"A");
    g.edit({12,20}).text = "┬"; write(g,{12,21},"B");
    // Explicit independent flat lines: 3/3 at row5; 1/1 at row15.
    for (int x = 5; x <= 12; ++x) g.edit({x,5}).text = "─";
    if (!coincident) {
        for (int x = 5; x <= 12; ++x) g.edit({x,15}).text = "━";
        g.edit({11,5}).text = "⠁"; // Bar1 + Line1
        g.edit({6,5}).text = "⠂";  // Bar2 + Line1
        g.edit({11,15}).text = "⠃";// Bar1 + Line2
        g.edit({6,15}).text = "⠄"; // Bar2 + Line2
        merman_handmade_composite_key(g,28,"⠁ Bar 1 + Line 1");
        merman_handmade_composite_key(g,29,"⠂ Bar 2 + Line 1");
        merman_handmade_composite_key(g,30,"⠃ Bar 1 + Line 2");
        merman_handmade_composite_key(g,31,"⠄ Bar 2 + Line 2");
    } else {
        // Both line owners occupy every row5 data cell. Their crossings with
        // each actual bar require a third owner; none can be inferred by color.
        for (int x = 5; x <= 12; ++x) g.edit({x,5}).text = "⠁";
        g.edit({11,5}).text = "⠂"; g.edit({6,5}).text = "⠃";
        merman_handmade_composite_key(g,28,"⠁ Line 1 + Line 2");
        merman_handmade_composite_key(g,29,"⠂ Bar 1 + Line 1 + Line 2");
        merman_handmade_composite_key(g,30,"⠃ Bar 2 + Line 1 + Line 2");
    }
    merman_handmade_composite_key(g,23,"█ Bar 1"); merman_handmade_composite_key(g,24,"░ Bar 2");
    merman_handmade_composite_key(g,25,"─ Line 1"); merman_handmade_composite_key(g,26,"━ Line 2");
    return g;
}
Grid merman_handmade_composite_header(Grid g,std::string_view caption) {
    Grid result{g.width,g.height+1,std::vector<Cell>(static_cast<std::size_t>(g.width*(g.height+1)))};
    for (int y = 0; y < g.height; ++y) for (int x = 0; x < g.width; ++x) result.edit({x,y+1}) = g.at({x,y});
    write(result,{0,0},caption); return result;
}
TEST(MermaidChartsMermanCompositeOracle, DistinctPairSymbolsAreLegendBoundRatherThanBitEncoded) {
    const auto model = merman_handmade_composite_model(); const auto good = merman_handmade_composite_scene();
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
    auto permutation = good;
    for (auto& cell : permutation.cells) {
        if (cell.text == "⠁") cell.text = "⠄";
        else if (cell.text == "⠄") cell.text = "⠁";
    }
    EXPECT_TRUE(merman_rectangular_errors(permutation,model).empty()) << explain(merman_rectangular_errors(permutation,model));
    auto wrong = good;
    for (auto& cell : wrong.cells) if (cell.text == "⠂") cell.text = "⠁";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good;
    wrong.edit({6,5}).text = "⠁"; wrong.edit({6,5}).foreground = "different RGB";
    wrong.edit({0,29}).text = "⠁"; wrong.edit({0,29}).foreground = "different RGB";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; merman_handmade_composite_key(wrong,28,"");
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; merman_handmade_composite_key(wrong,28,"⠁ Bar 2 + Line 1");
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; merman_handmade_composite_key(wrong,28,"⠁ Bar 1 + Line 1 + Line 2");
    // Global any-owner claims would miss this invented Line2 contribution.
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; merman_handmade_composite_key(wrong,28,"⠁ Bar 1 + Line 1 + Missing");
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.height += 1; wrong.cells.resize(static_cast<std::size_t>(wrong.width*wrong.height));
    merman_handmade_composite_key(wrong,32,"⠅ Bar 1 + Bar 2"); // unused key
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
}
TEST(MermaidChartsMermanCompositeOracle, FullyConnectedDetourCannotInventAFlatLinePeak) {
    const auto model = merman_handmade_composite_model(); const auto good = merman_handmade_composite_scene();
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty());
    auto wrong = good;
    // Source Line1 is flat3/3 at row5. This is a complete connected path with
    // the same source samples and crossing owners, not an isolated extra mark.
    wrong.edit({8,5}).text = " "; wrong.edit({9,5}).text = " ";
    wrong.edit({7,5}).text = "╯"; wrong.edit({7,4}).text = "╭";
    wrong.edit({8,4}).text = "─"; wrong.edit({9,4}).text = "─";
    wrong.edit({10,4}).text = "╮"; wrong.edit({10,5}).text = "╰";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
}
TEST(MermaidChartsMermanCompositeOracle, PairBoundingBoxDoesNotPermitCategoryBacktracking) {
    auto model = merman_handmade_composite_model();
    model.ownership.series[2].values = {1,3}; model.points[2] = {{0,1},{1,3}};
    auto good = merman_handmade_composite_scene();
    // Re-author Line1 as (5,15)->(9,15)->(9,5)->(12,5), independently of
    // production. Line2 remains flat at row15, including their shared stem.
    for (int x = 5; x <= 12; ++x) good.edit({x,5}).text = " ";
    good.edit({6,5}).text = "░"; good.edit({11,5}).text = "█";
    for (int x = 5; x <= 9; ++x) good.edit({x,15}).text = "⠁";
    for (int y = 6; y <= 14; ++y) good.edit({9,y}).text = "│";
    good.edit({9,5}).text = "╭"; good.edit({10,5}).text = "─";
    good.edit({11,5}).text = "⠃"; good.edit({12,5}).text = "─";
    good.edit({6,15}).text = "⠂"; good.edit({11,15}).text = "⠄";
    merman_handmade_composite_key(good,28,"⠁ Line 1 + Line 2");
    merman_handmade_composite_key(good,29,"⠂ Bar 2 + Line 1 + Line 2");
    merman_handmade_composite_key(good,30,"⠃ Bar 1 + Line 1");
    merman_handmade_composite_key(good,31,"⠄ Bar 1 + Line 2");
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
    auto wrong = good;
    for (int y = 5; y <= 14; ++y) wrong.edit({9,y}).text = " ";
    wrong.edit({10,5}).text = " "; wrong.edit({9,15}).text = "━";
    // Entire path remains inside x5..12/y5..15, but includes x8->7:
    // (5,15)->(8,15)->(8,12)->(7,12)->(7,9)->(10,9)->(10,5)->(12,5).
    for (int y : {13,14}) wrong.edit({8,y}).text = "│";
    wrong.edit({8,12}).text = "╮"; wrong.edit({7,12}).text = "╰";
    for (int y : {10,11}) wrong.edit({7,y}).text = "│";
    wrong.edit({7,9}).text = "╭";
    for (int x : {8,9}) wrong.edit({x,9}).text = "─";
    wrong.edit({10,9}).text = "╯";
    for (int y : {6,7,8}) wrong.edit({10,y}).text = "│";
    wrong.edit({10,5}).text = "╭";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
}
TEST(MermaidChartsMermanCompositeOracle, EveryDeclaredOwnerHasItsOwnVerifiedDataContribution) {
    const auto model = merman_handmade_composite_model(true); const auto good = merman_handmade_composite_scene(true);
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
    auto wrong = good; merman_handmade_composite_key(wrong,29,"⠂ Bar 1 + Line 2");
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; merman_handmade_composite_key(wrong,29,"⠂ Bar 1 + Bar 2 + Line 1 + Line 2");
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({9,5}).text = "⠀"; // forbidden U+2800
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({9,20}).text = "⠁"; // axes are not line data
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({9,9}).text = "⠁"; // ownerless extra plot marker
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({4,5}).text = "─"; // extra branch off the line
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({9,4}).text = "⠁"; // branch falsely sharing two real line owners
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({11,0}).text = " "; // missing full bar endpoint
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
}
TEST(MermaidChartsMermanCompositeOracle, CompositeKeysCannotBorrowAuthoredCaptionTokens) {
    auto model = merman_handmade_composite_model(); model.ownership.title = "⠁ Bar 1 + Line 1";
    const auto good = merman_handmade_composite_header(merman_handmade_composite_scene(),model.ownership.title);
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
    auto wrong = good; merman_handmade_composite_key(wrong,29,"");
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; merman_handmade_composite_key(wrong,29,"⠁ Line 1 + Bar 1");
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({0,26}).text = "⠁"; // primary line key role
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
}
TEST(MermaidChartsMermanCompositeOracle, KeysHaveOneCompleteFooterRoleAndOneSourceOrderedSet) {
    auto model = merman_handmade_composite_model(); auto good = merman_handmade_composite_scene();
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty());
    auto wrong = good; merman_handmade_composite_key(wrong,28,"⠁ Bar 1 + Line 1  ⠂ Bar 2 + Line 1");
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; merman_handmade_composite_key(wrong,28,"⠁ Bar 1 + Line 1 trailing");
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; merman_handmade_composite_key(wrong,28,"⠁ Bar 1 + Line 1 + Line 1");
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; merman_handmade_composite_key(wrong,31,"⠄ Bar 1 + Line 1");
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    // Same text in its source-authored axis role and actual footer role passes;
    // deleting the actual key cannot borrow the independently owned caption.
    model.ownership.category_title = "⠁ Bar 1 + Line 1";
    good.height += 1; good.cells.resize(static_cast<std::size_t>(good.width*good.height));
    for (int y = 31; y >= 23; --y) for (int x = 0; x < good.width; ++x) good.edit({x,y+1}) = good.at({x,y});
    merman_handmade_composite_key(good,23,""); write(good,{0,22},model.ownership.category_title);
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty());
    wrong = good; merman_handmade_composite_key(wrong,29,"");
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
}
std::string merman_handmade_braille(unsigned number) {
    const unsigned code = 0x2800+number;
    return {static_cast<char>(0xe0|(code>>12)),static_cast<char>(0x80|((code>>6)&0x3f)),static_cast<char>(0x80|(code&0x3f))};
}
TEST(MermaidChartsMermanCompositeOracle, FiniteCodecAdmitsEveryNonblankCellAndNoNextSymbol) {
    std::set<std::string> admitted;
    for (unsigned n = 1; n <= 255; ++n) {
        const auto token = merman_handmade_braille(n); SCOPED_TRACE(n);
        EXPECT_TRUE(braille_composite(token));
        EXPECT_EQ(ftxui::string_width(token),1);
        EXPECT_EQ(ftxui::Utf8ToGlyphs(token).size(),1u);
        admitted.insert(token);
    }
    EXPECT_EQ(admitted.size(),255u);
    EXPECT_FALSE(braille_composite(merman_handmade_braille(0)));
    EXPECT_FALSE(braille_composite(merman_handmade_braille(256)));
    for (const std::string& invalid : {std::string{},std::string("⠁ "),std::string("⠁́"),
            std::string("⠁⠂"),std::string("\xe2\xa0"),std::string("\xe2\xa0\xc0"),std::string("\xe2\xa4\x80")})
        EXPECT_FALSE(braille_composite(invalid));
    auto model = merman_handmade_composite_model(); auto good = merman_handmade_composite_scene();
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty());
    // Endpoints are valid symbol identities even without a bit-mask encoding.
    for (auto& cell : good.cells) if (cell.text == "⠁") cell.text = merman_handmade_braille(255);
    EXPECT_TRUE(merman_rectangular_errors(good,model).empty());
    for (auto& cell : good.cells) if (cell.text == merman_handmade_braille(255)) cell.text = merman_handmade_braille(256);
    EXPECT_FALSE(merman_rectangular_errors(good,model).empty());
    model.ownership.series[0].name = "Bar 1 + Line 1";
    EXPECT_EQ(merman_rectangular_profile(model).pending,"ambiguous_composite_owner_name_profile_pending");
    model = merman_handmade_composite_model(); model.ownership.series[1].name = model.ownership.series[0].name;
    EXPECT_EQ(merman_rectangular_profile(model).pending,"ambiguous_composite_owner_name_profile_pending");
}
TEST(MermaidChartsMermanCompositeOracle, VariableStyleMatchingAndTypedAllocationDebitSharedWork) {
    const auto model = merman_handmade_composite_model();
    Legend keys;
    for (unsigned n = 1; n <= 4; ++n) {
        Cell sample; sample.text = merman_handmade_braille(n);
        const std::array<std::uint32_t,4> masks{3,5,9,6};
        keys.styles.push_back({sample,5,masks[n-1]});
    }
    Cell unknown; unknown.text = "╋";
    Work short_scan{3}; EXPECT_THROW((void)owns(unknown,0,model.ownership,keys,short_scan),std::runtime_error);
    Work full_scan{8}; EXPECT_FALSE(owns(unknown,0,model.ownership,keys,full_scan)); EXPECT_EQ(full_scan.left,0u);
    Cell exact_key = keys.styles[2].sample;
    Work short_exact{3}; EXPECT_THROW((void)owns(exact_key,0,model.ownership,keys,short_exact),std::runtime_error);
    Work full_exact{4}; EXPECT_TRUE(owns(exact_key,0,model.ownership,keys,full_exact)); EXPECT_EQ(full_exact.left,0u);
    Grid small{2,2,std::vector<Cell>(4)}; Claims claims(small);
    Work short_init{3}; EXPECT_THROW(claims.enable_data(short_init),std::runtime_error); EXPECT_TRUE(claims.data_owners.empty());
    Work full_init{4}; claims.enable_data(full_init); EXPECT_EQ(full_init.left,0u); EXPECT_EQ(claims.data_owners.size(),4u);
    EXPECT_THROW(claims.data({0,0},0,full_init),std::runtime_error);
    Work first{1}; claims.data({0,0},0,first); EXPECT_EQ(first.left,0u); EXPECT_EQ(claims.data_owners[0],1u);
    Work second{1}; claims.data({0,0},2,second); EXPECT_EQ(claims.data_owners[0],5u);
    auto grid = merman_handmade_composite_scene(); Work axis_work; const auto axes_candidates = axes(grid,axis_work);
    ASSERT_FALSE(axes_candidates.empty());
    Work zero{0}; EXPECT_THROW((void)legend(grid,model.ownership,axes_candidates.front(),zero,true),std::runtime_error);
}
} // namespace

namespace {
TEST(MermaidChartsMermanCompositeResources, AmbiguousCompleteOwnerNamesKeepTheWholeSource) {
    for (const std::string source : {
        "xychart\nx-axis [A,B]\ny-axis 0 --> 10\nbar \"same\" [2,8]\nbar \"same\" [5,6]\nline [8,2]\nline [4,4]\n",
        "xychart\nx-axis [A,B]\ny-axis 0 --> 10\nbar \"Bar 1 + Line 1\" [2,8]\nbar [5,6]\nline [8,2]\nline [4,4]\n"})
        source_fallback(source);
}
} // namespace

namespace {
// These scenes are handwritten physical grids. Their source-derived model uses
// the same declared domain as the reviewed reference, never product output.
MermanXY merman_handmade_positive_baseline(bool horizontal = false) {
    auto model = merman_handmade_rectangular();
    model.ownership.horizontal = horizontal; model.ownership.domain = {{10,14}};
    model.ownership.series[0].values = {11,14}; model.points = {{{0,11},{1,14}}};
    model.authored_y = {{10,14}}; model.y_first = 10; model.y_last = 14;
    return model;
}
Grid merman_handmade_positive_vertical() {
    auto grid = merman_handmade_fixed_bars();
    for (int n = 0; n < 5; ++n) write(grid,{0,20-5*n},std::to_string(10+n));
    for (int y = 0; y < 20; ++y) grid.edit({5,y}).text = y >= 15 ? "█" : " ";
    return grid;
}
Grid merman_handmade_positive_horizontal() {
    Grid grid{48,10,std::vector<Cell>(480)};
    for (int y = 0; y <= 8; ++y) grid.edit({5,y}).text = "│";
    for (int x = 5; x <= 45; ++x) grid.edit({x,8}).text = "─";
    for (int n = 0; n < 5; ++n) {
        const int x = 5+10*n;
        grid.edit({x,8}).text = n ? "┬" : "┼";
        write(grid,{x-2,9},std::to_string(10+n)+".00");
    }
    for (const auto& [row,last,label] : {std::tuple{2,15,"A"},std::tuple{6,45,"B"}}) {
        grid.edit({5,row}).text = "├"; write(grid,{3,row},label);
        for (int x = 6; x <= last; ++x) grid.edit({x,row}).text = "█";
    }
    return grid;
}
TEST(MermaidChartsMermanPositiveBaselineOracle, VerticalBarsUseTheDeclaredLowerEndpointAndFullPhysicalCapacity) {
    const auto model = merman_handmade_positive_baseline(); const auto good = merman_handmade_positive_vertical();
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
    // val/max (excluded zero) would incorrectly paint sixteen cells for11/14.
    auto wrong = good;
    for (int y = 4; y < 15; ++y) wrong.edit({5,y}).text = "█";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({5,19}).text = " ";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({5,14}).text = "█";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({5,16}).text = " ";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good;
    for (int y = 15; y < 20; ++y) { wrong.edit({5,y}).text = " "; wrong.edit({6,y}).text = "█"; }
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; write(wrong,{0,20},"09");
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; write(wrong,{0,0},"15");
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({2,15}).text = "│";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    EXPECT_FALSE(merman_rectangular_errors(scene({"A 11","B 14","10 11 12 13 14"}),model).empty());
    wrong = good; wrong.edit({7,12}).text = "█";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
}
TEST(MermaidChartsMermanPositiveBaselineOracle, HorizontalBarsHaveTheSameReferenceAndIndependentFortyCellMap) {
    const auto model = merman_handmade_positive_baseline(true); const auto good = merman_handmade_positive_horizontal();
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
    auto wrong = good;
    for (int x = 16; x <= 36; ++x) wrong.edit({x,2}).text = "█";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({6,2}).text = " ";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({15,2}).text = " ";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({16,2}).text = "█";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good;
    for (int x = 6; x <= 15; ++x) { wrong.edit({x,2}).text = " "; wrong.edit({x,3}).text = "█"; }
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; write(wrong,{3,9},"09.00");
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({25,8}).text = "─";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({5,2}).text = "│";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
}
TEST(MermaidChartsMermanPositiveBaselineOracle, TrueZeroHeightRetainsItsRealSlotTickAndNoInventedInk) {
    for (const bool horizontal : {false,true}) {
        auto model = merman_handmade_positive_baseline(horizontal);
        model.ownership.series[0].values[0] = 10; model.points[0][0].value = 10;
        auto good = horizontal ? merman_handmade_positive_horizontal() : merman_handmade_positive_vertical();
        if (horizontal) for (int x = 6; x <= 15; ++x) good.edit({x,2}).text = " ";
        else for (int y = 15; y < 20; ++y) good.edit({5,y}).text = " ";
        ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
        const Point port = horizontal ? Point{5,2} : Point{5,20};
        const Point label = horizontal ? Point{3,2} : Point{5,21};
        auto wrong = good; wrong.edit(horizontal ? Point{6,2} : Point{5,19}).text = "█";
        EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
        wrong = good; wrong.edit(label).text = " ";
        EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
        for (const std::string& broken : horizontal ? std::vector<std::string>{"│","┌","└","┼"} : std::vector<std::string>{"─","┐","┌","┼"}) {
            wrong = good; wrong.edit(port).text = broken;
            EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
        }
    }
}
TEST(MermaidChartsMermanPositiveBaselineOracle, MixedLinesKeepTheirOriginalAffineAndExactSharedEndpointOwners) {
    auto model = merman_handmade_positive_baseline();
    model.ownership.series.push_back({"line","Line 1",{12,13}});
    model.points.push_back({{0,12},{1,13}}); model.named_series.push_back(false);
    Grid good{32,28,std::vector<Cell>(32u*28u)};
    const auto base = merman_handmade_positive_vertical();
    for (int y = 0; y < base.height; ++y) for (int x = 0; x < base.width; ++x) good.edit({x,y}) = base.at({x,y});
    for (int x = 5; x <= 8; ++x) good.edit({x,10}).text = "─";
    for (int y = 5; y <= 10; ++y) good.edit({8,y}).text = "│";
    for (int x = 8; x <= 11; ++x) good.edit({x,5}).text = "─";
    good.edit({8,10}).text = "╯"; good.edit({8,5}).text = "╭";
    good.edit({11,5}).text = "⊕";
    write(good,{0,23},"█ Bar 1"); write(good,{0,24},"─ Line 1"); write(good,{0,26},"⊕ Bar 1 + Line 1");
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
    auto wrong = good; wrong.edit({8,7}).text = " ";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({5,10}).text = " "; wrong.edit({5,3}).text = "─";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; write(wrong,{0,26},"⊕ Line 1         ");
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({11,6}).text = "⊕";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({7,9}).text = "│";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
}
TEST(MermaidChartsMermanPositiveBaselineOracle, GroupedOffsetsAndCompositeMasksCannotChooseDifferentReferences) {
    auto model = merman_handmade_composite_model();
    model.authored_y = {{10,14}}; model.y_first = 10; model.y_last = 14; model.ownership.domain = {{10,14}};
    for (auto& series : model.ownership.series) for (auto& value : series.values) value += 10;
    for (auto& points : model.points) for (auto& point : points) *point.value += 10;
    auto good = merman_handmade_composite_scene();
    for (int n = 0; n < 5; ++n) write(good,{0,20-5*n},std::to_string(10+n));
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
    auto wrong = good; wrong.edit({4,12}).text = " "; wrong.edit({5,12}).text = "█";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good;
    for (int y = 0; y < 20; ++y) for (int x = 3; x <= 14; ++x) {
        auto& ink = wrong.edit({x,y}).text;
        if (ink == "█") ink = "░"; else if (ink == "░") ink = "█";
    }
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({6,19}).text = " ";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
}
TEST(MermaidChartsMermanPositiveBaselineOracle, PhysicalRoundingNeverTurnsNonbaselineSourceValuesIntoMissingData) {
    auto model = merman_handmade_positive_baseline();
    auto good = merman_handmade_positive_vertical();
    for (int y = 0; y < 20; ++y) good.edit({5,y}).text = " ";
    // The exact half-cell uses global round(physical position), not a separately
    // rounded height. Its upper neighbour has one visible cell and must pass.
    model.ownership.series[0].values[0] = std::nextafter(10.1,14.0);
    model.points[0][0].value = model.ownership.series[0].values[0];
    good.edit({5,19}).text = "█";
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
    good.edit({5,19}).text = " ";
    for (const double tiny : {10.1,std::nextafter(10.1,10.0),std::nextafter(10.0,14.0)}) {
        model.ownership.series[0].values[0] = tiny; model.points[0][0].value = tiny;
        EXPECT_FALSE(merman_rectangular_errors(good,model).empty());
        auto wrong = good; wrong.edit({5,20}).text = "⊕";
        EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
        wrong = good; wrong.edit({5,20}).text = "█";
        EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    }
    model = merman_handmade_positive_baseline(true); good = merman_handmade_positive_horizontal();
    for (int x = 6; x <= 15; ++x) good.edit({x,2}).text = " ";
    model.ownership.series[0].values[0] = std::nextafter(10.0,14.0); model.points[0][0].value = model.ownership.series[0].values[0];
    EXPECT_FALSE(merman_rectangular_errors(good,model).empty());
    good.edit({5,2}).text = "⊕";
    EXPECT_FALSE(merman_rectangular_errors(good,model).empty());
}
TEST(MermaidChartsSemantics, CompleteMermanPositiveBaseline15Batch) {
    // The source-first structural32 and first-reason15 were independently
    // reviewed before renderer observation. Other dependencies stay pending.
    const std::set<std::string> expected_ids{
        "merman/0fda862e7d39ad81ff9d","merman/26b0cd5e3c3bcf2c2d0c","merman/334a9b8332ecd21f2677",
        "merman/3390df4173003c7ed0f9","merman/468d4936e16c6a14d94b","merman/47bf83c00ac7e8730b46",
        "merman/849085d741016c9335f0","merman/93f0e9dadd2c5c259dd3","merman/9bc72e53b583e12e01d4",
        "merman/ada5297b3962a5dd64f0","merman/b5cecba0def962c8ac4a","merman/b604437a8712ffdbffb4",
        "merman/be76c5e68481288670b7","merman/cbd65c7f26c8e2a3ff40","merman/f8d89363e95de9e9505f"};
    json::JsonMutDoc report; auto root = report.object(); auto rows = root.make_arr();
    root.set("kind","complete_source_first_positive_baseline15_of_structural32");
    root.set("source_facts_sha256","a370aafe6a8adb0bb4abed662ba3241c25bf515777c50bac6104d6622dd5877d");
    root.set("qualification","Geometry-only native profile; source fallback is unmet. Complete102 and108 original assertion applications remain separate.");
    std::set<std::string> visited; int structural = 0,ascending_positive = 0,matches = 0,fallbacks = 0,geometry_errors = 0,oracle_errors = 0;
    ASSERT_NO_THROW(merman_xy102_native_batch([&](JsonVal row,JsonVal,const MermanXY& model) {
        const bool bars = std::ranges::any_of(model.ownership.series,[](const auto& series) { return series.type == "bar"; });
        if (!model.authored_y || model.y_first == 0 || !bars) return;
        ++structural;
        const bool positive = model.y_first > 0 && model.y_first < model.y_last;
        ascending_positive += positive;
        const auto profile = merman_rectangular_profile(model);
        const auto id = row.get_string("id");
        const bool active = positive && profile.pending.empty();
        EXPECT_EQ(active,expected_ids.contains(id)) << id;
        auto item = root.make_obj(); item.set("id",id); item.set("input_sha256",digest(row.get_string("input")));
        item.set("eligible_positive_baseline",active); item.set("independent_profile_pending",profile.pending);
        if (active) {
            SCOPED_TRACE(id); visited.insert(id);
            bool is_source = false,oracle_error = false;
            std::vector<std::string> errors;
            try {
                const auto source = row.get_string("input");
                auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
                const auto wide = capture(element,240,256);
                is_source = same_source_surface(wide,capture(loom::ui::render_markdown(fenced(source,"text")),240,256));
                errors = is_source ? std::vector<std::string>{"Complete authored source retained; positive-domain bar geometry unmet"} : merman_rectangular_errors(wide,model);
                const auto narrow = capture(element,40,256);
                const bool narrow_source = same_source_surface(narrow,capture(loom::ui::render_markdown(fenced(source,"text")),40,256));
                if (!is_source && errors.empty() && !narrow_source) {
                    const auto narrow_errors = merman_rectangular_errors(narrow,model);
                    for (const auto& error : narrow_errors) errors.push_back("Narrow positive-domain geometry: "+error);
                }
                const auto restored = capture(element,240,256); EXPECT_TRUE(restored == wide);
                EXPECT_NE(wide.text().find(kSentinel),std::string::npos); EXPECT_NE(narrow.text().find(kSentinel),std::string::npos);
                if (!(restored == wide)) errors.push_back("Positive-baseline width restoration changed the complete surface");
                if (wide.text().find(kSentinel) == std::string::npos) errors.push_back("Positive-baseline wide render lost following content");
                if (narrow.text().find(kSentinel) == std::string::npos) errors.push_back("Positive-baseline narrow render lost following content");
                item.set("restored_identically",restored == wide);
            } catch (const std::exception& error) {
                oracle_error = true; errors.push_back("Explicit positive-baseline oracle error: "+std::string(error.what()));
            }
            // A resource/codec failure is distinct from wrong geometry; every
            // remaining authored source still executes and reaches the report.
            if (oracle_error) ++oracle_errors;
            else if (is_source) ++fallbacks;
            else if (errors.empty()) ++matches;
            else ++geometry_errors;
            EXPECT_TRUE(errors.empty()) << explain(errors);
            item.set("outcome",oracle_error ? "oracle_error" : (is_source ? "source_fallback" : (errors.empty() ? "geometry_match_pending_review" : "geometry_error")));
            auto failures = root.make_arr(); for (const auto& error : errors) failures.append(root.make_str(error)); item.add("errors",failures);
        }
        rows.append(item);
    }));
    EXPECT_EQ(structural,32); EXPECT_EQ(ascending_positive,30); EXPECT_EQ(visited,expected_ids);
    EXPECT_EQ(matches+fallbacks+geometry_errors+oracle_errors,15);
    root.set("structural_sources",structural); root.set("ascending_positive_sources",ascending_positive);
    root.set("feature_sources",static_cast<int>(visited.size())); root.set("geometry_matches_pending_review",matches);
    root.set("source_fallback",fallbacks); root.set("geometry_errors",geometry_errors); root.set("oracle_errors",oracle_errors);
    root.set("certified_complete_geometry",0); root.set("original_global_function_applications_pending",108);
    root.add("cases",rows); report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto path = std::filesystem::path(LOOM_MERMAID_REPORT_DIR)/"mermaid-merman-positive-baseline15-semantics.json";
    std::ofstream out(path,std::ios::binary); ASSERT_TRUE(out && (out << report.to_pretty_string() << '\n')) << "Cannot write " << path;
#endif
}
} // namespace

namespace {
TEST(MermaidChartsMermanPositiveBaselineOracle, AllReferenceBarOnlyAxesNeedASeparateZeroSpanOutcome) {
    for (const bool horizontal : {false,true}) {
        auto model = merman_handmade_positive_baseline(horizontal);
        model.ownership.series[0].values = {10,10}; model.points = {{{0,10},{1,10}}};
        auto grid = horizontal ? merman_handmade_positive_horizontal() : merman_handmade_positive_vertical();
        if (horizontal) {
            for (const int row : {2,6}) for (int x = 6; x <= 45; ++x) grid.edit({x,row}).text = " ";
        } else for (const int column : {5,11}) for (int y = 0; y < 20; ++y) grid.edit({column,y}).text = " ";
        EXPECT_EQ(merman_rectangular_profile(model).pending,"positive_reference_bar_only_zero_span_outcome_profile_pending");
        EXPECT_FALSE(merman_rectangular_errors(grid,model).empty());
        // A constant line can carry real path geometry; bar-only is essential
        // to this admission boundary. Reference-axis line ink is not inferred.
        model.ownership.series.push_back({"line","Line 1",{10,10}});
        model.points.push_back({{0,10},{1,10}}); model.named_series.push_back(false);
        EXPECT_TRUE(merman_rectangular_profile(model).pending.empty());
    }
}
} // namespace

namespace {
TEST(MermaidChartsMermanPositiveBaselineResources, RealSourcesSeparateOwnedDataFromReferenceAxisAndEmptyBars) {
    for (const bool horizontal : {false,true}) {
        const std::string header = std::string("xychart")+(horizontal ? " horizontal" : "")+"\nx-axis [A,B]\ny-axis 10 --> 14\n";
        // Zero bar-only outcomes and tiny unowned bar marks still need their
        // own profile. Keep every original source-policy example in place.
        for (const std::string& body : std::vector<std::string>{
            "bar [10,10]\n","bar [10.000000000000002,14]\n"}) {
            SCOPED_TRACE(header+body); source_fallback(header+body);
        }
        auto complete_mixed_geometry = [&](double near,std::string_view literal) {
            const std::string source = header+"bar [11,14]\nline ["+std::string(literal)+",13]\n";
            SCOPED_TRACE(source);
            auto model = merman_handmade_positive_baseline(horizontal);
            model.ownership.series.push_back({"line","Line 1",{near,13}});
            model.points.push_back({{0,near},{1,13}}); model.named_series.push_back(false);
            auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
            auto ordinary = loom::ui::render_markdown(fenced(source,"text"));
            const auto wide = capture(element,240,256);
            EXPECT_FALSE(same_source_surface(wide,capture(ordinary,240,256)));
            const auto errors = merman_rectangular_errors(wide,model);
            EXPECT_TRUE(errors.empty()) << explain(errors);
            const auto narrow = capture(element,40,256);
            if (!same_source_surface(narrow,capture(ordinary,40,256))) {
                const auto narrow_errors = merman_rectangular_errors(narrow,model);
                EXPECT_TRUE(narrow_errors.empty()) << explain(narrow_errors);
            }
            EXPECT_TRUE(capture(element,240,256) == wide);
            EXPECT_NE(wide.text().find(kSentinel),std::string::npos);
            EXPECT_NE(narrow.text().find(kSentinel),std::string::npos);
        };
        // These same four valid sources used to be explicitly unsupported.
        // Their source values/models stay intact; the reviewed shared-axis
        // projection now requires complete owner geometry instead of fallback.
        complete_mixed_geometry(10,"10");
        complete_mixed_geometry(10.000000000000002,"10.000000000000002");
        // Keep the previous one-physical-cell-off-axis positive unchanged.
        complete_mixed_geometry(horizontal ? 10.1 : 10.2,horizontal ? "10.1" : "10.2");
    }
}
} // namespace

namespace {
MermanXY merman_orientation_source_model(bool horizontal,bool cjk = false) {
    auto model = merman_handmade_rectangular();
    model.ownership.horizontal = horizontal;
    model.ownership.title = cjk ? "方向" : "Orientation";
    model.ownership.category_title = cjk ? "类别" : "Category";
    model.ownership.value_title = cjk ? "数值" : "Value";
    model.category_names = cjk ? std::vector<std::string>{"甲","乙"} : std::vector<std::string>{"A","B"};
    model.ownership.series.push_back({"line","Line 1",{3,1}});
    model.points.push_back({{0,3},{1,1}}); model.named_series.push_back(false);
    return model;
}
std::string merman_orientation_source(std::string_view scalar,std::string_view header,bool beta = false,bool cjk = false) {
    std::string source;
    if (!scalar.empty()) source = "---\nconfig:\n  xyChart:\n    chartOrientation: "+std::string(scalar)+"\n---\n";
    source += beta ? "xychart-beta" : "xychart";
    if (!header.empty()) source += " "+std::string(header);
    source += cjk ? "\ntitle \"方向\"\nx-axis \"类别\" [\"甲\",\"乙\"]\ny-axis \"数值\" 0 --> 4\n" :
        "\ntitle \"Orientation\"\nx-axis \"Category\" [A,B]\ny-axis \"Value\" 0 --> 4\n";
    return source+"bar [2,4]\nline [3,1]\n";
}
std::vector<std::string> merman_orientation_source_errors(std::string_view source,const MermanXY& model) {
    std::vector<std::string> errors;
    auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
    auto ordinary = loom::ui::render_markdown(fenced(source,"text"));
    const auto wide = capture(element,240,256);
    if (same_source_surface(wide,capture(ordinary,240,256))) errors.push_back("Configured orientation retained as source; geometry unmet");
    else errors = merman_rectangular_errors(wide,model);
    const auto narrow = capture(element,40,256);
    if (!same_source_surface(narrow,capture(ordinary,40,256))) {
        const auto narrow_errors = merman_rectangular_errors(narrow,model);
        for (const auto& error : narrow_errors) errors.push_back("Narrow configured orientation: "+error);
    }
    if (!(capture(element,240,256) == wide)) errors.push_back("Configured orientation width restoration changed the complete surface");
    if (wide.text().find(kSentinel) == std::string::npos) errors.push_back("Configured orientation wide render lost following content");
    if (narrow.text().find(kSentinel) == std::string::npos) errors.push_back("Configured orientation narrow render lost following content");
    return errors;
}
TEST(MermaidChartsMermanOrientationOracle, BareHeaderRetainsDefaultOrConfiguredStringEnum) {
    const auto default_source = merman_orientation_source("","");
    EXPECT_TRUE(merman_orientation_source_errors(default_source,merman_orientation_source_model(false)).empty());
    for (const auto& [scalar,horizontal] : std::vector<std::pair<std::string,bool>>{
        {"horizontal",true},{"vertical",false},{"'horizontal'",true},{"'vertical'",false},
        {"\"horizontal\"",true},{"\"vertical\"",false}}) {
        const auto source = merman_orientation_source(scalar,""); SCOPED_TRACE(source);
        const auto errors = merman_orientation_source_errors(source,merman_orientation_source_model(horizontal));
        EXPECT_TRUE(errors.empty()) << explain(errors);
    }
}
TEST(MermaidChartsMermanOrientationOracle, ExplicitHeaderOverridesEachMetadataDirectionAndEveryAxisRole) {
    for (const bool beta : {false,true}) for (const auto& [scalar,header,horizontal] :
        std::vector<std::tuple<std::string,std::string,bool>>{
            {"horizontal","vertical",false},{"vertical","horizontal",true},
            {"horizontal","horizontal",true},{"vertical","vertical",false}}) {
        const auto source = merman_orientation_source(scalar,header,beta); SCOPED_TRACE(source);
        const auto expected = merman_orientation_source_model(horizontal);
        const auto errors = merman_orientation_source_errors(source,expected);
        EXPECT_TRUE(errors.empty()) << explain(errors);
        const auto wide = capture(loom::ui::render_markdown(fenced(source,"mermaid")),240,256);
        auto wrong = expected; wrong.ownership.horizontal = !horizontal;
        EXPECT_FALSE(merman_rectangular_errors(wide,wrong).empty());
        wrong = expected; std::swap(wrong.ownership.category_title,wrong.ownership.value_title);
        EXPECT_FALSE(merman_rectangular_errors(wide,wrong).empty());
    }
}
TEST(MermaidChartsMermanOrientationOracle, ConfiguredHorizontalUnicodeRetainsContinuationAndDataOwnership) {
    const auto source = merman_orientation_source("horizontal","",false,true);
    const auto expected = merman_orientation_source_model(true,true);
    const auto errors = merman_orientation_source_errors(source,expected);
    EXPECT_TRUE(errors.empty()) << explain(errors);
    const auto wide = capture(loom::ui::render_markdown(fenced(source,"mermaid")),240,256);
    Work work;
    for (const std::string& label : std::vector<std::string>{"甲","乙","方向","类别","数值"}) {
        const auto at = find_text(wide,label,work); ASSERT_EQ(at.size(),1u) << label;
        EXPECT_TRUE(wide.at({at.front().x+1,at.front().y}).text.empty());
    }
}
TEST(MermaidChartsMermanOrientationResources, InvalidEnumsAliasesAndDuplicateMapsKeepTheCompleteSource) {
    for (const std::string& scalar : std::vector<std::string>{"sideways","Horizontal","true","1","null","{}","[]","|","&axis horizontal","*axis","\"horizontal\\n\""})
        source_fallback(merman_orientation_source(scalar,"horizontal"));
    const std::string body = merman_orientation_source("","");
    for (const std::string& metadata : std::vector<std::string>{
        "config:\n  xyChart:\n    chartOrientation: horizontal\n    chartOrientation: horizontal\n",
        "config:\n  xyChart:\n    chartOrientation: horizontal\n    chartOrientation: vertical\n",
        "config:\n  xyChart:\n    chartOrientation: horizontal\n  xyChart:\n    chartOrientation: vertical\n",
        "config:\n  xyChart:\n    chartOrientation: horizontal\nconfig:\n  xyChart:\n    chartOrientation: vertical\n",
        "config:\n  xychart:\n    chartOrientation: horizontal\n",
        "config:\n  xyChart:\n    orientation: horizontal\n",
        "config:\n  chartOrientation: horizontal\n",
        "config:\n  xyChart:\n    chartOrientation: horizontal\n    orientation: vertical\n",
        "config:\n  xyChart:\n    \"chartOrientation\": horizontal\n",
        "config:\n  xyChart:\n    chartOrientation:\n"}) source_fallback("---\n"+metadata+"---\n"+body);
    source_fallback(merman_orientation_source("horizontal","vertical horizontal"));
    source_fallback("%%{init: {\"xyChart\": {\"chartOrientation\": \"horizontal\"}}}%%\n"+body);
}
} // namespace

namespace {
TEST(MermaidChartsSemantics, CompleteMermanOrientation10InventoryAndEligibleTwoGeometry) {
    // Membership is the independently reviewed canonical source property,
    // not observed art. Eight other dependencies remain explicitly unmet.
    const std::set<std::string> expected_ids{
        "merman/1465019ee2ee57a21217","merman/334a9b8332ecd21f2677","merman/76934683ae5dc045dbae",
        "merman/9c890fc9196c5893c9ea","merman/b50974f5288abac7cfe4","merman/b7e2e0438a13f8dff541",
        "merman/be76c5e68481288670b7","merman/e129388167382a04e9f9","merman/f50b2db43a0801593459",
        "merman/f5f012ca4e7055701b83"};
    const std::set<std::string> expected_eligible{
        "merman/334a9b8332ecd21f2677","merman/be76c5e68481288670b7"};
    json::JsonMutDoc report; auto root = report.object(); auto rows = root.make_arr();
    root.set("kind","complete_source_orientation10_inventory_with_eligible2_geometry_gate");
    root.set("source_facts_sha256","510a302b27cac54167e78f6299e3ac3426a95aa1a804167fce5c8c8bacc614a2");
    root.set("qualification","All10 source identities observed. Two eligible profiles require full native geometry; eight dependencies stay unmet and are not successful drawing tests. Full102/global108 original applications remain separate.");
    std::set<std::string> visited,eligible; int fields = 0,pending = 0,matches = 0,active_source = 0,geometry_errors = 0,oracle_errors = 0,eligible_oracle_errors = 0;
    int source_count = 0,candidate_count = 0,observation_errors = 0;
    ASSERT_NO_THROW(merman_xy102_native_batch([&](JsonVal row,JsonVal fact,const MermanXY& model) {
        bool orientation = false;
        fact.get("authored_config_fields").iter([&](JsonVal field) {
            const auto path = field.get("path");
            if (path.size() == 2 && path.at(0).as_str() == "xyChart" && path.at(1).as_str() == "chartOrientation") {
                orientation = true; EXPECT_EQ(field.get_string("value"),"horizontal");
            }
        });
        if (!orientation) return;
        const auto id = row.get_string("id"); SCOPED_TRACE(id); visited.insert(id);
        fields += static_cast<int>(fact.get("authored_config_fields").size());
        EXPECT_TRUE(model.ownership.horizontal);
        const auto profile = merman_rectangular_profile(model);
        const bool active = profile.pending.empty(); EXPECT_EQ(active,expected_eligible.contains(id));
        if (active) eligible.insert(id); else ++pending;
        auto item = root.make_obj(); item.set("id",id); item.set("input_sha256",digest(row.get_string("input")));
        item.set("eligible_geometry_profile",active); item.set("dependency_pending",profile.pending);
        bool is_source = false,observed = false,oracle_error = false;
        std::vector<std::string> errors;
        try {
            const auto source = row.get_string("input");
            auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
            auto ordinary = loom::ui::render_markdown(fenced(source,"text"));
            const auto wide = capture(element,240,256);
            is_source = same_source_surface(wide,capture(ordinary,240,256)); observed = true;
            if (active) {
                if (is_source) errors.push_back("Authored orientation retained as source; eligible horizontal geometry unmet");
                else errors = merman_rectangular_errors(wide,model);
            } else errors.push_back("Explicit orientation-cohort dependency remains unmet: "+profile.pending);
            const auto narrow = capture(element,40,256);
            const bool narrow_source = same_source_surface(narrow,capture(ordinary,40,256));
            if (active && !is_source && errors.empty() && !narrow_source) {
                const auto narrow_errors = merman_rectangular_errors(narrow,model);
                for (const auto& error : narrow_errors) errors.push_back("Narrow configured orientation: "+error);
            }
            const bool restored = capture(element,240,256) == wide;
            EXPECT_TRUE(restored); item.set("restored_identically",restored);
            if (!restored) errors.push_back("Orientation cohort width restoration changed the complete surface");
            const bool wide_sentinel = wide.text().find(kSentinel) != std::string::npos;
            const bool narrow_sentinel = narrow.text().find(kSentinel) != std::string::npos;
            EXPECT_TRUE(wide_sentinel); EXPECT_TRUE(narrow_sentinel);
            if (!wide_sentinel || !narrow_sentinel) errors.push_back("Orientation cohort lost following content");
        } catch (const std::exception& error) {
            oracle_error = true; errors.push_back("Explicit orientation oracle error: "+std::string(error.what()));
        }
        if (observed) { if (is_source) ++source_count; else ++candidate_count; } else ++observation_errors;
        if (oracle_error) { ++oracle_errors; if (active) ++eligible_oracle_errors; ADD_FAILURE() << explain(errors); }
        else if (active) {
            if (is_source) ++active_source; else if (errors.empty()) ++matches; else ++geometry_errors;
            EXPECT_TRUE(errors.empty()) << explain(errors);
        }
        item.set("observed",observed ? (is_source ? "source_fallback" : "candidate_art") : "observation_error");
        item.set("outcome",oracle_error ? "oracle_error" : (!active ? "dependency_pending" :
            (is_source ? "source_fallback" : (errors.empty() ? "geometry_match_pending_review" : "geometry_error"))));
        auto failures = root.make_arr(); for (const auto& error : errors) failures.append(root.make_str(error)); item.add("errors",failures);
        rows.append(item);
    }));
    EXPECT_EQ(visited,expected_ids); EXPECT_EQ(eligible,expected_eligible); EXPECT_EQ(fields,75); EXPECT_EQ(pending,8);
    EXPECT_EQ(matches+active_source+geometry_errors+eligible_oracle_errors,static_cast<int>(eligible.size()));
    EXPECT_EQ(source_count+candidate_count+observation_errors,static_cast<int>(visited.size()));
    root.set("complete_source_inputs",static_cast<int>(visited.size())); root.set("eligible_geometry_inputs",static_cast<int>(eligible.size()));
    root.set("other_dependency_inputs_pending",pending); root.set("authored_config_leaf_applications",fields);
    root.set("geometry_matches_pending_review",matches); root.set("eligible_source_fallback",active_source);
    root.set("geometry_errors",geometry_errors); root.set("oracle_errors",oracle_errors); root.set("eligible_oracle_errors",eligible_oracle_errors);
    root.set("source_fallback_observations",source_count); root.set("candidate_art_observations",candidate_count); root.set("observation_errors",observation_errors);
    root.set("certified_complete_geometry",0); root.set("original_smoke_function_cohort_applications_pending",10);
    root.set("original_global_function_applications_pending",108); root.add("cases",rows); report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto path = std::filesystem::path(LOOM_MERMAID_REPORT_DIR)/"mermaid-merman-orientation10-semantics.json";
    std::ofstream out(path,std::ios::binary); ASSERT_TRUE(out && (out << report.to_pretty_string() << '\n')) << "Cannot write " << path;
#endif
}
} // namespace

namespace {
MermanXY merman_handmade_shared_axis_model(bool horizontal,bool constant = false,bool second = false) {
    auto model = merman_handmade_positive_baseline(horizontal);
    model.ownership.series[0].values = {11,11}; model.points[0] = {{0,11},{1,11}};
    model.ownership.series.push_back({"line","Line 1",constant ? std::vector<double>{10,10} : std::vector<double>{10.01,12}});
    model.points.push_back(constant ? std::vector<MermanPoint>{{0,10},{1,10}} : std::vector<MermanPoint>{{0,10.01},{1,12}});
    model.named_series.push_back(false);
    if (second) {
        model.ownership.series.push_back({"line","Line 2",{13,13}});
        model.points.push_back({{0,13},{1,13}}); model.named_series.push_back(false);
    }
    return model;
}
Grid merman_handmade_shared_axis_scene(bool horizontal,bool constant = false,bool second = false) {
    const auto base = horizontal ? merman_handmade_positive_horizontal() : merman_handmade_positive_vertical();
    Grid grid{48,32,std::vector<Cell>(48u*32u)};
    for (int y = 0; y < base.height; ++y) for (int x = 0; x < base.width; ++x) grid.edit({x,y}) = base.at({x,y});
    // Independently authored two-category bars: each reaches 11 in [10,14].
    if (horizontal) for (int x = 16; x <= 45; ++x) grid.edit({x,6}).text = " ";
    else for (int y = 0; y < 15; ++y) grid.edit({11,y}).text = " ";
    const std::array<std::pair<std::string_view,std::string_view>,11> doubles{{
        {"─","═"},{"│","║"},{"┌","╔"},{"┐","╗"},{"└","╚"},{"┘","╝"},
        {"├","╠"},{"┤","╣"},{"┬","╦"},{"┴","╩"},{"┼","╬"}}};
    for (int y = 0; y < base.height; ++y) for (int x = 0; x < base.width; ++x)
        for (const auto& [plain,boxed] : doubles) if (base.at({x,y}).text == plain) grid.edit({x,y}).text = boxed;
    const int key = horizontal ? 11 : 23;
    write(grid,{0,key},"█ Bar 1"); write(grid,{0,key+1},"─ Line 1");
    if (second) write(grid,{0,key+2},"━ Line 2");
    if (!constant) write(grid,{0,key+2+(second ? 1 : 0)},"Line 1 A=10.01");
    if (horizontal) {
        if (constant) {
            for (int y = 2; y <= 6; ++y) grid.edit({5,y}).text = (y == 2 || y == 6) ? "├" : "│";
        } else {
            grid.edit({5,2}).text = "├"; grid.edit({5,3}).text = "│"; grid.edit({5,4}).text = "├";
            for (int x = 6; x < 25; ++x) grid.edit({x,4}).text = "─";
            grid.edit({25,4}).text = "╮"; grid.edit({25,5}).text = "│"; grid.edit({25,6}).text = "│";
        }
        if (second) for (int y = 2; y <= 6; ++y) grid.edit({35,y}).text = "┃";
    } else {
        if (constant) {
            for (int x = 5; x <= 11; ++x) grid.edit({x,20}).text = (x == 5 || x == 11) ? "┬" : "─";
        } else {
            grid.edit({5,20}).text = "┬";
            for (int x = 6; x < 8; ++x) grid.edit({x,20}).text = "─";
            grid.edit({8,20}).text = "┴";
            for (int y = 11; y < 20; ++y) grid.edit({8,y}).text = "│";
            grid.edit({8,10}).text = "╭";
            for (int x = 9; x <= 11; ++x) grid.edit({x,10}).text = "─";
        }
        if (second) for (int x = 5; x <= 11; ++x) grid.edit({x,5}).text = "━";
    }
    return grid;
}
TEST(MermaidChartsMermanSharedAxisOracle, EveryDoubleGlyphHasExactIndependentFamilyAndBudget) {
    constexpr std::array<std::pair<std::string_view,int>,11> expected{{
        {"═",L|R},{"║",U|D},{"╔",R|D},{"╗",L|D},{"╚",U|R},{"╝",U|L},
        {"╠",U|D|R},{"╣",U|D|L},{"╦",L|R|D},{"╩",L|R|U},{"╬",U|D|L|R}}};
    for (const auto& [glyph,bits] : expected) {
        EXPECT_EQ(stroke(glyph).bits,bits); EXPECT_EQ(stroke(glyph).family,6);
        EXPECT_NE(stroke(glyph).family,stroke("─").family); EXPECT_NE(stroke(glyph).family,stroke("━").family);
        EXPECT_NE(stroke(glyph).family,stroke("┄").family);
        EXPECT_FALSE(line_style(Cell{std::string(glyph),{},{}},Style{Cell{"─",{},{}},1,1}));
        EXPECT_FALSE(line_style(Cell{std::string(glyph),{},{}},Style{Cell{"━",{},{}},2,2}));
    }
    const auto model = merman_handmade_shared_axis_model(true); const auto grid = merman_handmade_shared_axis_scene(true);
    auto profile = merman_rectangular_profile(model); ASSERT_TRUE(profile.pending.empty()); Work source_work;
    profile.view.chart.shared_axis = shared_axis_source(profile.view.chart,source_work); ASSERT_TRUE(profile.view.chart.shared_axis);
    Work work; const Axes axis{5,0,8,45}; const auto calibrated = scale(grid,axis,true,work); ASSERT_TRUE(calibrated);
    const auto categories = anchors(grid,profile.view.chart,axis,work); ASSERT_TRUE(categories);
    Work short_init{grid.cells.size()*5-1};
    EXPECT_THROW(shared_axis_plan(grid,profile.view.chart,axis,*calibrated,*categories,short_init),std::runtime_error);
    Work exactly_init{grid.cells.size()*5};
    EXPECT_THROW(shared_axis_plan(grid,profile.view.chart,axis,*calibrated,*categories,exactly_init),std::runtime_error);
    EXPECT_EQ(exactly_init.left,0u);
    Work bounded_work{grid.cells.size()*5+32};
    EXPECT_THROW(shared_axis_plan(grid,profile.view.chart,axis,*calibrated,*categories,bounded_work),std::runtime_error);
}
TEST(MermaidChartsMermanSharedAxisOracle, HorizontalQuantizedSampleKeepsExactOwnerValueAndAllPorts) {
    const auto model = merman_handmade_shared_axis_model(true); const auto good = merman_handmade_shared_axis_scene(true);
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
    for (const auto& [point,glyph] : std::vector<std::pair<Point,std::string>>{
        {{5,2},"╠"},{{5,2},"┫"},{{5,2},"┬"},{{5,2},"┴"},{{5,2},"│"},{{5,3},"║"},
        {{5,3},"┼"},{{5,1},"│"},{{5,4},"╬"},{{5,4},"┼"},{{5,0},"╠"}}) {
        auto wrong = good; wrong.edit(point).text = glyph; SCOPED_TRACE(glyph);
        EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    }
    auto wrong = good; for (int x = 0; x < wrong.width; ++x) wrong.edit({x,13}).text = " ";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; write(wrong,{0,13},"Line 1 A=10   "); EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; write(wrong,{0,13},"Line 1 B=10.01"); EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({25,5}).text = " "; EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
}
TEST(MermaidChartsMermanSharedAxisOracle, VerticalUnionPreservesTickDownportAndVerifiedNorthDataLeg) {
    const auto model = merman_handmade_shared_axis_model(false); const auto good = merman_handmade_shared_axis_scene(false);
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
    for (const auto& [point,glyph] : std::vector<std::pair<Point,std::string>>{
        {{5,20},"╦"},{{5,20},"─"},{{5,20},"┼"},{{8,20},"─"},{{8,20},"┼"},{{8,20},"┤"},
        {{6,20},"═"},{{7,20},"━"},{{4,20},"─"},{{11,20},"┼"},{{2,10},"│"}}) {
        auto wrong = good; wrong.edit(point).text = glyph; SCOPED_TRACE(glyph);
        EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    }
    auto wrong = good; wrong.edit({8,11}).text = " "; EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({9,19}).text = "│"; EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
}
TEST(MermaidChartsMermanSharedAxisOracle, ConstantReferenceSeriesNeedsItsEntireOwnerPathRatherThanFrame) {
    for (const bool horizontal : {false,true}) {
        const auto model = merman_handmade_shared_axis_model(horizontal,true);
        const auto good = merman_handmade_shared_axis_scene(horizontal,true);
        ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
        auto wrong = good;
        if (horizontal) for (int y = 2; y <= 6; ++y) wrong.edit({5,y}).text = (y == 2 || y == 6) ? "╠" : "║";
        else for (int x = 5; x <= 11; ++x) wrong.edit({x,20}).text = (x == 5 || x == 11) ? "╦" : "═";
        EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
        wrong = good; wrong.edit(horizontal ? Point{5,3} : Point{7,20}).text = horizontal ? "║" : "═";
        EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
        wrong = good; wrong.edit(horizontal ? Point{5,1} : Point{4,20}).text = horizontal ? "│" : "─";
        EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
        wrong = good; wrong.edit(horizontal ? Point{5,3} : Point{7,20}).text = horizontal ? "┃" : "━";
        EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
        auto multiple = model; multiple.ownership.series.push_back({"line","Line 2",{10,10}});
        multiple.points.push_back({{0,10},{1,10}}); multiple.named_series.push_back(false);
        EXPECT_FALSE(merman_rectangular_errors(good,multiple).empty());
    }
}
TEST(MermaidChartsMermanSharedAxisOracle, TwoLineKeysAndAxisCannotSupplyAnAbsentOwnerOrCaption) {
    for (const bool horizontal : {false,true}) {
        const auto model = merman_handmade_shared_axis_model(horizontal,false,true);
        const auto good = merman_handmade_shared_axis_scene(horizontal,false,true);
        ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
        const int key = horizontal ? 11 : 23;
        auto wrong = good; write(wrong,{2,key+1},"Line 2"); write(wrong,{2,key+2},"Line 1");
        EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
        wrong = good; write(wrong,{0,key+1},"═ Line 1"); EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
        wrong = good; wrong.edit(horizontal ? Point{5,2} : Point{5,20}).text = "⊕";
        EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
        wrong = good; if (horizontal) for (int y = 2; y <= 6; ++y) wrong.edit({35,y}).text = " ";
        else for (int x = 5; x <= 11; ++x) wrong.edit({x,5}).text = " ";
        EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
        auto wrong_owner = model; std::swap(wrong_owner.ownership.series[1].values,wrong_owner.ownership.series[2].values);
        std::swap(wrong_owner.points[1],wrong_owner.points[2]);
        EXPECT_FALSE(merman_rectangular_errors(good,wrong_owner).empty());
    }
}
TEST(MermaidChartsMermanSharedAxisOracle, FullDetoursWrongMidpointsAndForeignStemsStayUnclaimed) {
    const auto model = merman_handmade_shared_axis_model(false); const auto good = merman_handmade_shared_axis_scene(false);
    ASSERT_TRUE(merman_rectangular_errors(good,model).empty()) << explain(merman_rectangular_errors(good,model));
    auto wrong = good;
    // A complete monotone route through x7 still reaches every sample, but its
    // source-declared axis departure midpoint is x8, not x7.
    for (int y = 10; y < 20; ++y) wrong.edit({8,y}).text = " ";
    wrong.edit({8,20}).text = "═"; wrong.edit({7,20}).text = "┴";
    for (int y = 11; y < 20; ++y) wrong.edit({7,y}).text = "│";
    wrong.edit({7,10}).text = "╭"; wrong.edit({8,10}).text = "─";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({8,18}).text = "├"; wrong.edit({9,18}).text = "─";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    // Independent reviewer counterexample: a claimed glyph's unused R arm
    // must fail even without a second unclaimed character beside it.
    wrong = good; wrong.edit({8,15}).text = "├";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    // A legitimate terminal straight cap remains visible, but an unverified
    // terminal tee and a fake four-way crossing do not inherit that exception.
    wrong = good; wrong.edit({11,10}).text = "┤";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({8,15}).text = "╋";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({8,10}).text = "├"; // real D|R turn plus an unverified U arm
    EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    auto three_model = model;
    three_model.category_names.push_back("C");
    three_model.ownership.series[0].values.push_back(11); three_model.points[0].push_back({2,11});
    three_model.ownership.series[1].values.push_back(12); three_model.points[1].push_back({2,12});
    auto three = good;
    for (int x = 15; x <= 20; ++x) three.edit({x,20}).text = "═";
    three.edit({17,20}).text = "╦"; write(three,{17,21},"C");
    for (int y = 15; y < 20; ++y) three.edit({17,y}).text = "█";
    for (int x = 12; x <= 17; ++x) three.edit({x,10}).text = "─";
    // B is now an internal source sample; the B->C constant-value step has a
    // zero-length middle leg, and still needs every real horizontal edge.
    ASSERT_TRUE(merman_rectangular_errors(three,three_model).empty()) << explain(merman_rectangular_errors(three,three_model));
    wrong = three; wrong.edit({11,10}).text = "┬";
    EXPECT_FALSE(merman_rectangular_errors(wrong,three_model).empty());
    wrong = three; wrong.edit({14,10}).text = "┴";
    EXPECT_FALSE(merman_rectangular_errors(wrong,three_model).empty());
    const auto horizontal_model = merman_handmade_shared_axis_model(true);
    const auto horizontal_good = merman_handmade_shared_axis_scene(true);
    ASSERT_TRUE(merman_rectangular_errors(horizontal_good,horizontal_model).empty());
    wrong = horizontal_good; wrong.edit({15,4}).text = "┬";
    EXPECT_FALSE(merman_rectangular_errors(wrong,horizontal_model).empty());
    const auto constant_model = merman_handmade_shared_axis_model(false,true);
    wrong = merman_handmade_shared_axis_scene(false,true);
    // Connected fake peak: x6->row19->x10->row20. Correct endpoints survive.
    for (int x = 6; x <= 10; ++x) wrong.edit({x,20}).text = "═";
    wrong.edit({6,20}).text = "┴"; wrong.edit({10,20}).text = "┴";
    wrong.edit({6,19}).text = "╭"; wrong.edit({10,19}).text = "╮";
    for (int x = 7; x < 10; ++x) wrong.edit({x,19}).text = "─";
    EXPECT_FALSE(merman_rectangular_errors(wrong,constant_model).empty());
}
TEST(MermaidChartsMermanSharedAxisOracle, UnknownAxisOwnersExtraPlotsAndSourceLimitsStayExplicit) {
    for (const bool horizontal : {false,true}) {
        auto near_model = merman_handmade_shared_axis_model(horizontal);
        near_model.ownership.series[1].values[0] = 10.000000000000002;
        near_model.points[1][0].value = 10.000000000000002;
        auto near_good = merman_handmade_shared_axis_scene(horizontal);
        const int row = horizontal ? 13 : 25;
        for (int x = 0; x < near_good.width; ++x) near_good.edit({x,row}).text = " ";
        write(near_good,{0,row},"Line 1 A=10.000000000000002");
        ASSERT_TRUE(merman_rectangular_errors(near_good,near_model).empty()) << explain(merman_rectangular_errors(near_good,near_model));
        auto rounded_to_reference = near_good;
        for (int x = 0; x < rounded_to_reference.width; ++x) rounded_to_reference.edit({x,row}).text = " ";
        write(rounded_to_reference,{0,row},"Line 1 A=10");
        EXPECT_FALSE(merman_rectangular_errors(rounded_to_reference,near_model).empty());
        // A real authored chart caption is independently claimed text; its
        // correct numeric content cannot replace the required footer readout.
        auto caption_model = near_model; caption_model.ownership.title = "Line 1 A=10.000000000000002";
        Grid caption{near_good.width,near_good.height+1,std::vector<Cell>(static_cast<std::size_t>(near_good.width*(near_good.height+1)))};
        for (int y = 0; y < near_good.height; ++y) for (int x = 0; x < near_good.width; ++x)
            caption.edit({x,y+1}) = near_good.at({x,y});
        write(caption,{0,0},caption_model.ownership.title);
        ASSERT_TRUE(merman_rectangular_errors(caption,caption_model).empty()) << explain(merman_rectangular_errors(caption,caption_model));
        for (int x = 0; x < caption.width; ++x) caption.edit({x,row+1}).text = " ";
        EXPECT_FALSE(merman_rectangular_errors(caption,caption_model).empty());
    }
    const auto model = merman_handmade_shared_axis_model(true); const auto good = merman_handmade_shared_axis_scene(true);
    auto wrong = good; write(wrong,{0,29},"extra data"); EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    wrong = good; wrong.edit({6,3}).text = "─"; EXPECT_FALSE(merman_rectangular_errors(wrong,model).empty());
    auto three = merman_handmade_shared_axis_model(true,false,true);
    three.ownership.series.push_back({"line","Line 3",{14,14}}); three.points.push_back({{0,14},{1,14}}); three.named_series.push_back(false);
    EXPECT_EQ(merman_rectangular_profile(three).pending,"extended_line_keys_and_overlap_sets_profile_pending");
    auto invalid_value = model; invalid_value.ownership.series[1].values[1] = std::numeric_limits<double>::infinity();
    EXPECT_FALSE(merman_rectangular_errors(good,invalid_value).empty());
    auto wrong_count = model; wrong_count.ownership.series[1].values.push_back(12);
    EXPECT_FALSE(merman_rectangular_errors(good,wrong_count).empty());
    auto null_model = model; null_model.points[1][0].value.reset();
    EXPECT_EQ(merman_rectangular_profile(null_model).pending,"retained_null_band_slot_profile_pending");
    auto ragged = model; ragged.points[1].push_back({2,13});
    EXPECT_EQ(merman_rectangular_profile(ragged).pending,"ragged_per_series_numeric_coordinates_profile_pending");
    auto all_bar = merman_handmade_positive_baseline(); all_bar.ownership.series[0].values = {10,10};
    all_bar.points[0] = {{0,10},{1,10}};
    EXPECT_EQ(merman_rectangular_profile(all_bar).pending,"positive_reference_bar_only_zero_span_outcome_profile_pending");
}
MermanXY merman_named_tail_source_model(std::string name) {
    auto model = merman_handmade_rectangular();
    model.ownership.series = {{"bar",std::move(name),{1,2}},{"bar","Costs",{3,4}}};
    model.points = {{{0,1},{1,2}},{{0,3},{1,4}}}; model.named_series = {true,true};
    return model;
}
TEST(MermaidChartsMermanNamedBandResources, SourceTitlesAndBandTailHaveTheirPinnedIndependentMeaning) {
    for (const auto& [title,expected] : std::vector<std::pair<std::string,std::string>>{
        {"Revenue One","RevenueOne"},{"\" Revenue  One \"","Revenue  One"},{"RevenueOne","RevenueOne"}}) {
        const auto model = merman_named_tail_source_model(expected);
        const auto source = "xychart\nx-axis [A,B]\ny-axis 0 --> 4\nbar "+title+" [1,2]\nbar \"Costs\" [3,4]\n";
        const auto errors = merman_orientation_source_errors(source,model);
        EXPECT_TRUE(errors.empty()) << source << explain(errors);
        const auto tail = "xychart\nx-axis [A,B]\ny-axis 0 --> 4\nbar "+title+" [1,2,999]\nbar \"Costs\" [3,4,-999]\n";
        const auto tail_errors = merman_orientation_source_errors(tail,model);
        EXPECT_TRUE(tail_errors.empty()) << tail << explain(tail_errors);
        auto inferred_model = model; inferred_model.authored_y.reset();
        const auto inferred_tail = "xychart\nx-axis [A,B]\nbar "+title+" [1,2,999]\nbar \"Costs\" [3,4,-999]\n";
        const auto inferred_errors = merman_orientation_source_errors(inferred_tail,inferred_model);
        EXPECT_TRUE(inferred_errors.empty()) << inferred_tail << explain(inferred_errors);
    }
    for (const std::string& source : std::vector<std::string>{
        "xychart\nx-axis [A,B]\nbar Revenue@One [1,2]\nbar \"Costs\" [3,4]",
        "xychart\nx-axis [A,B]\nbar title [1,2]\nbar \"Costs\" [3,4]",
        "xychart\nx-axis [A,B]\nbar \"broken [1,2]\nbar \"Costs\" [3,4]",
        "xychart\nx-axis [A,B]\nbar Revenue [1,2,wat]\nbar \"Costs\" [3,4]",
        "xychart\nx-axis [A,B]\nbar Revenue [1,2,1e309]\nbar \"Costs\" [3,4]",
        "xychart\nx-axis [A,B,C]\nbar Revenue [1,2]\nbar \"Costs\" [3,4,5]",
        "xychart\nx-axis 0 --> 2\nbar Revenue [1,2]\nbar \"Costs\" [3,4,5]"}) source_fallback(source);
}
TEST(MermaidChartsSemantics, CompleteMermanNamedBandAxis13SourceUnionAndEligibleTwoGeometry) {
    const std::set<std::string> expected_ids{
        "merman/0cfc570b349a718a0f18","merman/1c6eefd22f3dc00ae3fc","merman/38399d9b6f84b66f397b",
        "merman/506f4a1e821982f95384","merman/7c40b524079c65f8b63f","merman/83af26d25cc380d2005d",
        "merman/88050d11f4a8870db912","merman/93f0e9dadd2c5c259dd3","merman/aa1b960c08021c182bc4",
        "merman/c72edeefc2511714d434","merman/ec60b8a277bb36d02d78","merman/f248b6991b99619a837a",
        "merman/f8d89363e95de9e9505f"};
    const std::set<std::string> unquoted_ids{
        "merman/1c6eefd22f3dc00ae3fc","merman/7c40b524079c65f8b63f","merman/88050d11f4a8870db912",
        "merman/93f0e9dadd2c5c259dd3","merman/aa1b960c08021c182bc4","merman/f248b6991b99619a837a",
        "merman/f8d89363e95de9e9505f"};
    const std::set<std::string> expected_eligible{"merman/93f0e9dadd2c5c259dd3","merman/f8d89363e95de9e9505f"};
    json::JsonMutDoc report; auto root = report.object(); auto rows = root.make_arr();
    root.set("kind","complete_source_named_band_axis13_union_with_two_geometry_obligations");
    root.set("source_facts_sha256","a93da19ca0263cf9c2b1da60007de2353aa3d0044885188d707f928174aa4700");
    root.set("qualification","Source union13 complete. Two eligible profiles require full typed native geometry; eleven independent dependencies stay pending. No original foreign API or complete family certification is implied.");
    std::set<std::string> visited,eligible;
    int u = 0,t = 0,m = 0,a = 0,tails = 0,nulls = 0,fields = 0,pending = 0,matches = 0,source_count = 0,candidates = 0;
    int active_source = 0,geometry_errors = 0,oracle_errors = 0,active_oracle_errors = 0,observation_errors = 0;
    ASSERT_NO_THROW(merman_xy102_native_batch([&](JsonVal row,JsonVal fact,const MermanXY& model) {
        const auto id = row.get_string("id"); if (!expected_ids.contains(id)) return; SCOPED_TRACE(id); visited.insert(id);
        const bool unquoted = unquoted_ids.contains(id),tail = fact.get("source_model_data_transformations").size() != 0;
        const bool missing = fact.get("band_missing_sample_slots").size() != 0;
        const auto profile = merman_rectangular_profile(model); const bool active = profile.pending.empty();
        Work source_work; const bool axis = active && model.authored_y && shared_axis_source(profile.view.chart,source_work);
        u += unquoted; t += tail; m += missing; a += axis; nulls += static_cast<int>(fact.get("band_missing_sample_slots").size());
        fact.get("source_model_data_transformations").iter([&](JsonVal transform) { tails += static_cast<int>(transform.get("omitted_authored_tail").size()); });
        fields += static_cast<int>(fact.get("authored_config_fields").size());
        EXPECT_EQ(active,expected_eligible.contains(id)); if (active) eligible.insert(id); else ++pending;
        auto item = root.make_obj(); item.set("id",id); item.set("input_sha256",digest(row.get_string("input")));
        item.set("primary_model_sha256",fact.get("primary_model_reference").get_string("sha256"));
        item.set("unquoted_source_title",unquoted); item.set("band_tail_transform",tail); item.set("missing_band_slots",missing);
        item.set("source_axis_line_sample",axis); item.set("eligible_geometry",active); item.set("dependency_pending",profile.pending);
        bool observed = false,is_source = false,oracle_error = false; std::vector<std::string> errors;
        try {
            const auto source = row.get_string("input"); auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
            auto ordinary = loom::ui::render_markdown(fenced(source,"text")); const auto wide = capture(element,240,256);
            is_source = same_source_surface(wide,capture(ordinary,240,256)); observed = true;
            if (!active) errors.push_back("Explicit independent source-union dependency: "+profile.pending);
            else if (is_source) errors.push_back("Eligible source name/band tail/axis ownership retained as source; geometry unmet");
            else errors = merman_rectangular_errors(wide,model);
            const auto narrow = capture(element,40,256); const bool narrow_source = same_source_surface(narrow,capture(ordinary,40,256));
            if (active && !is_source && errors.empty() && !narrow_source) {
                for (const auto& error : merman_rectangular_errors(narrow,model)) errors.push_back("Narrow shared-axis geometry: "+error);
            }
            const bool restored = capture(element,240,256) == wide;
            const bool sentinel = wide.text().find(kSentinel) != std::string::npos && narrow.text().find(kSentinel) != std::string::npos;
            EXPECT_TRUE(restored); EXPECT_TRUE(sentinel); item.set("restored_identically",restored); item.set("following_content_retained",sentinel);
            if (!restored) errors.push_back("Named/band/axis source union changed the restored surface");
            if (!sentinel) errors.push_back("Named/band/axis source union lost following content");
        } catch (const std::exception& error) { oracle_error = true; errors.push_back("Explicit named/band/axis oracle error: "+std::string(error.what())); }
        if (!observed) ++observation_errors; else if (is_source) ++source_count; else ++candidates;
        if (oracle_error) { ++oracle_errors; if (active) ++active_oracle_errors; ADD_FAILURE() << explain(errors); }
        else if (active) {
            if (is_source) ++active_source; else if (errors.empty()) ++matches; else ++geometry_errors;
            EXPECT_TRUE(errors.empty()) << explain(errors);
        }
        item.set("observed",observed ? (is_source ? "source_fallback" : "candidate_art") : "observation_error");
        item.set("outcome",oracle_error ? "oracle_error" : (!active ? "dependency_pending" : (is_source ? "source_fallback" : (errors.empty() ? "geometry_match_pending_review" : "geometry_error"))));
        auto failures = root.make_arr(); for (const auto& error : errors) failures.append(root.make_str(error)); item.add("errors",failures); rows.append(item);
    }));
    EXPECT_EQ(visited,expected_ids); EXPECT_EQ(eligible,expected_eligible); EXPECT_EQ(u,7); EXPECT_EQ(t,6); EXPECT_EQ(m,2); EXPECT_EQ(a,2);
    EXPECT_EQ(tails,22); EXPECT_EQ(nulls,3); EXPECT_EQ(fields,1); EXPECT_EQ(pending,11);
    EXPECT_EQ(source_count+candidates+observation_errors,13);
    EXPECT_EQ(matches+active_source+geometry_errors+active_oracle_errors,2);
    root.set("complete_source_inputs",static_cast<int>(visited.size())); root.set("eligible_geometry_inputs",static_cast<int>(eligible.size()));
    root.set("other_dependency_inputs_pending",pending); root.set("unquoted_title_inputs",u); root.set("unquoted_title_applications",16);
    root.set("band_tail_inputs",t); root.set("omitted_tail_values",tails); root.set("missing_band_inputs",m); root.set("missing_band_slots",nulls);
    root.set("shared_axis_line_inputs",a); root.set("authored_config_leaf_applications",fields);
    root.set("geometry_matches_pending_review",matches); root.set("eligible_source_fallback",active_source);
    root.set("geometry_errors",geometry_errors); root.set("oracle_errors",oracle_errors); root.set("eligible_oracle_errors",active_oracle_errors); root.set("source_fallback_observations",source_count);
    root.set("candidate_art_observations",candidates); root.set("observation_errors",observation_errors);
    root.set("certified_complete_geometry",0); root.set("original_smoke_function_cohort_applications_pending",13);
    root.set("original_global_function_applications_pending",108); root.add("cases",rows); report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto path = std::filesystem::path(LOOM_MERMAID_REPORT_DIR)/"mermaid-merman-named-band-axis13-semantics.json";
    std::ofstream out(path,std::ios::binary); ASSERT_TRUE(out && (out << report.to_pretty_string() << '\n')) << "Cannot write " << path;
#endif
}
} // namespace

namespace {
// Value labels are validated against their original physical cells. The oracle
// never restores hypothetical fill and then mistakes that reconstruction for ink.
struct MermanValuePolicy {
    MermanRectangularProfile base;
    bool compact = false,outside = false;
};
MermanValuePolicy merman_value_policy(const MermanXY& source) {
    auto copy = source; copy.show_values = false;
    MermanValuePolicy policy{merman_rectangular_profile(copy),false,source.outside_values};
    if (!source.show_values) policy.base.pending = "value_labels_not_requested_by_source";
    policy.compact = source.ownership.series.size() == 1 && source.ownership.series.front().type == "bar";
    if (!source.band) policy.base.pending = "independent_numeric_coordinate_profile_pending";
    return policy;
}
bool merman_value_active(const MermanXY& model) {
    return model.show_values && merman_value_policy(model).base.pending.empty();
}
std::string value_record(const XY& model,std::size_t owner,std::size_t sample) {
    return model.series[owner].name+" "+model.categories[sample]+"="+numeric_x_label(model.series[owner].values[sample]);
}
XYValuePlan merman_value_plan(const Grid& grid,const XY& model,const Axes& axis,const Scale& calibrated,
                             const std::vector<int>& categories,const Legend& keys,bool compact,bool outside,
                             Work& work,std::vector<std::string>& errors) {
    work.spend(grid.cells.size()*2);
    XYValuePlan plan{grid.width,std::vector<int>(grid.cells.size(),-1),{}, {}};
    std::vector<bool> reserved(grid.cells.size());
    auto add_token = [&](Point first,const std::string& text,std::size_t owner,std::size_t sample,bool overlay) {
        work.spend(text.size()); const auto glyphs = ftxui::Utf8ToGlyphs(text);
        if (glyphs.empty() || glyphs.size() > 256 || ftxui::string_width(text) > 256)
            throw std::runtime_error("Value label exceeds the unchanged label bound");
        bool valid = true;
        for (std::size_t n = 0; n < glyphs.size(); ++n) {
            work.spend(); const Point at{first.x+static_cast<int>(n),first.y};
            if (!grid.has(at)) { valid = false; continue; }
            const auto index = static_cast<std::size_t>(at.y*grid.width+at.x);
            if (reserved[index] || grid.at(at).text != glyphs[n]) valid = false;
        }
        // A numeric token cannot be borrowed as the prefix of a larger token.
        if (numeric_token(text)) {
            auto number_part = [](std::string_view cell) {
                return cell.size() == 1 && ((cell[0] >= '0' && cell[0] <= '9') || (cell[0] >= 'a' && cell[0] <= 'z') ||
                    (cell[0] >= 'A' && cell[0] <= 'Z') || cell == "_" || cell == "." || cell == "-" || cell == "+" || cell == "%");
            };
            if (number_part(grid.at({first.x-1,first.y}).text) ||
                number_part(grid.at({first.x+static_cast<int>(glyphs.size()),first.y}).text)) valid = false;
        }
        if (!valid) { errors.push_back("Value token is absent, truncated, reused, shifted or not a whole exact numeric identity"); return; }
        const int reference = static_cast<int>(std::round(calibrated.project(xy_bar_reference(model))));
        const int end = static_cast<int>(std::round(calibrated.project(model.series[owner].values[sample])));
        for (std::size_t n = 0; n < glyphs.size(); ++n) {
            work.spend(); const Point at{first.x+static_cast<int>(n),first.y};
            const auto index = static_cast<std::size_t>(at.y*grid.width+at.x); reserved[index] = true;
            if (!overlay) continue;
            // Only this exact source bar's required cells can be replaced. A
            // neighboring source sample is forbidden even when its owner agrees.
            const bool own = model.horizontal ? at.y == categories[sample] && at.x > std::min(reference,end) && at.x <= std::max(reference,end) :
                at.x == categories[sample] && at.y >= std::min(reference,end) && at.y < std::max(reference,end);
            if (own) plan.bar_overlay[index] = static_cast<int>(owner);
            for (std::size_t other = 0; other < categories.size(); ++other) {
                work.spend(); if (other == sample) continue;
                const int other_end = static_cast<int>(std::round(calibrated.project(model.series[owner].values[other])));
                const bool occupies = model.horizontal ? at.y == categories[other] && at.x > std::min(reference,other_end) && at.x <= std::max(reference,other_end) :
                    at.x == categories[other] && at.y >= std::min(reference,other_end) && at.y < std::max(reference,other_end);
                if (occupies) errors.push_back("Inside value label erases a different source category's bar");
            }
            if (at.x == axis.x || at.y == axis.y)
                errors.push_back("Inside value label erases a real axis/tick connection");
        }
        plan.tokens.emplace_back(first,text); plan.bindings.emplace_back(owner,sample);
    };
    if (compact) {
        const auto& values = model.series.front().values;
        const int baseline = static_cast<int>(std::round(calibrated.project(xy_bar_reference(model))));
        const int top_captions = static_cast<int>(!model.title.empty())+
            static_cast<int>(!(model.horizontal ? model.category_title : model.value_title).empty());
        for (std::size_t sample = 0; sample < values.size(); ++sample) {
            work.spend(); const auto token = numeric_x_label(values[sample]);
            const int token_width = ftxui::string_width(token),end = static_cast<int>(std::round(calibrated.project(values[sample])));
            const int length = std::abs(end-baseline);
            if (!length || values[sample] == xy_bar_reference(model)) {
                errors.push_back("Zero-height value placement has no reviewed independently visible bar"); continue;
            }
            const bool external = outside || (model.horizontal && token_width >= length);
            Point at;
            if (model.horizontal) at = {external ? axis.right+2 : end-token_width+1,categories[sample]};
            else at = {categories[sample],external ? axis.top-1 : end};
            if (!model.horizontal && external && at.y < top_captions)
                errors.push_back("Outside data-value row overlaps a real chart/axis caption role");
            add_token(at,token,0,sample,!external);
            int ink = 0;
            const int step = end > baseline ? 1 : -1;
            for (int position = baseline+step; position != end+step; position += step) {
                work.spend(); const Point mark = model.horizontal ? Point{position,categories[sample]} : Point{categories[sample],position};
                if (!plan.covers(mark,0) && owns(grid.at(mark),0,model,keys,work)) ++ink;
            }
            if (!ink) errors.push_back("A numeric label consumed every independent nonzero bar mark");
        }
        for (int row = xy_native_key_start(model,axis); row < grid.height; ++row) {
            work.spend(static_cast<std::size_t>(grid.width)); const auto text = trim(grid.row(row));
            if (!text.empty() && text != kSentinel) errors.push_back("Compact bar label cannot invent a supplementary owner/value footer");
        }
    } else {
        int row = xy_native_key_start(model,axis);
        if (model.series.size() > 1) {
            for (std::size_t owner = 0; owner < model.series.size(); ++owner) {
                work.spend();
                if (owner >= keys.primary.size() || keys.primary[owner] != Point{2,row+static_cast<int>(owner)})
                    errors.push_back("Value footer borrows a caption/readout instead of its ordered primary key");
            }
            row += static_cast<int>(model.series.size());
            for (const int composite_row : keys.composite_rows) {
                work.spend(); if (composite_row != row++) errors.push_back("Value footer composite key rows are not independent and consecutive");
            }
        }
        for (std::size_t owner = 0; owner < model.series.size(); ++owner)
            for (std::size_t sample = 0; sample < model.categories.size(); ++sample) {
                work.spend(); const auto text = value_record(model,owner,sample);
                if (row >= grid.height || trim(grid.row(row)) != text)
                    errors.push_back("Value footer is not an exact complete source-ordered owner/category/value record");
                add_token({0,row++},text,owner,sample,false);
            }
        for (; row < grid.height; ++row) {
            work.spend(static_cast<std::size_t>(grid.width)); const auto text = trim(grid.row(row));
            if (!text.empty() && text != kSentinel) errors.push_back("Value footer has an extra unknown/reused readout or plot");
        }
    }
    return plan;
}
std::vector<std::string> merman_value_errors(const Grid& original,const MermanXY& source,Work& work) {
    const auto policy = merman_value_policy(source);
    if (!policy.base.pending.empty()) return {"Independent value-label source dependency: "+policy.base.pending};
    const auto grid = merman_rectangular_compact(original,work);
    auto expected = policy.base.view.chart; expected.shared_axis = source.authored_y && shared_axis_source(expected,work);
    std::vector<std::string> best{"Value-labelled chart has no complete fixed-capacity physical axes"};
    int bars = 0,width = 0;
    for (const auto& series : expected.series) bars += series.type == "bar";
    for (const auto& category : expected.categories) width = std::max(width,ftxui::string_width(category));
    const int stride = expected.horizontal ? std::max(4,2*bars+2) : std::max({6,2*bars+3,width+3});
    for (const auto& axis : axes(grid,work)) {
        const auto calibrated = scale(grid,axis,expected.horizontal,work);
        const auto ports = anchors(grid,expected,axis,work); const auto keys = legend(grid,expected,axis,work,true);
        if (!calibrated || !ports || !keys) continue;
        bool finite = true;
        for (const auto& owner : expected.series) for (const double value : owner.values) {
            work.spend(); finite &= std::isfinite(value) && value >= calibrated->minimum && value <= calibrated->maximum;
        }
        if (!finite) continue;
        std::vector<std::string> errors;
        const auto labels = merman_value_plan(grid,expected,axis,*calibrated,*ports,*keys,policy.compact,policy.outside,work,errors);
        if (errors.empty()) {
            work.spend(grid.cells.size());
            errors = xy_candidate_errors(grid,expected,axis,work,true,&labels);
            const int first = expected.horizontal ? axis.top+2 : axis.x+stride/2;
            const int last = first+static_cast<int>(expected.categories.size()-1)*stride;
            if ((expected.horizontal && (axis.right-axis.x != 40 || axis.y != last+std::max(2,bars))) ||
                (!expected.horizontal && (axis.y-axis.top != 20 || axis.right != last+stride/2)))
                errors.push_back("Value labels changed the reviewed source-fixed plot capacity/extent");
            for (std::size_t n = 0; n < ports->size(); ++n) {
                work.spend(); if ((*ports)[n] != first+static_cast<int>(n)*stride) errors.push_back("Value label moved its category rather than binding its original slot");
            }
            merman_rectangular_source_constraints(grid,expected,axis,work,errors,&labels);
        }
        if (errors.empty()) return {};
        if (best.front() == "Value-labelled chart has no complete fixed-capacity physical axes" || errors.size() < best.size()) best = std::move(errors);
    }
    return best;
}
std::vector<std::string> merman_value_errors(const Grid& grid,const MermanXY& model) {
    Work work; return merman_value_errors(grid,model,work);
}
Grid value_handmade_resize(const Grid& source,int width,int height) {
    Grid result{width,height,std::vector<Cell>(static_cast<std::size_t>(width*height))};
    bounded(result);
    for (int y = 0; y < std::min(source.height,height); ++y)
        for (int x = 0; x < std::min(source.width,width); ++x) result.edit({x,y}) = source.at({x,y});
    return result;
}
Grid value_handmade_shift(const Grid& source,int rows) {
    Grid result{source.width,source.height+rows,std::vector<Cell>(static_cast<std::size_t>(source.width*(source.height+rows)))};
    bounded(result);
    for (int y = 0; y < source.height; ++y) for (int x = 0; x < source.width; ++x) result.edit({x,y+rows}) = source.at({x,y});
    return result;
}
MermanXY value_handmade_model(bool horizontal = false) {
    auto model = merman_handmade_positive_baseline(horizontal); model.show_values = true; return model;
}
Grid value_handmade_inside(bool horizontal = false) {
    auto g = value_handmade_resize(horizontal ? merman_handmade_positive_horizontal() : merman_handmade_positive_vertical(),64,32);
    if (horizontal) { write(g,{14,2},"11"); write(g,{44,6},"14"); }
    else { write(g,{5,15},"11"); write(g,{11,0},"14"); }
    return g;
}
void value_handmade_footer(Grid& grid,const MermanXY& model,int first) {
    int row = first; auto expected = model.ownership; expected.categories = model.category_names;
    for (std::size_t owner = 0; owner < model.ownership.series.size(); ++owner)
        for (std::size_t sample = 0; sample < model.category_names.size(); ++sample)
            write(grid,{0,row++},value_record(expected,owner,sample));
}
MermanXY value_handmade_line_model() {
    auto model = merman_handmade_rectangular(); model.show_values = true;
    model.ownership.series = {{"line","Line 1",{1,3}}}; model.points = {{{0,1},{1,3}}}; return model;
}
Grid value_handmade_line() {
    auto g = value_handmade_resize(merman_handmade_fixed_bars(),64,36);
    for (int y = 0; y < 20; ++y) { g.edit({5,y}).text = " "; g.edit({11,y}).text = " "; }
    for (int x = 5; x <= 8; ++x) g.edit({x,15}).text = "─";
    for (int y = 5; y <= 15; ++y) g.edit({8,y}).text = "│";
    g.edit({8,15}).text = "┘"; g.edit({8,5}).text = "┌";
    for (int x = 9; x <= 11; ++x) g.edit({x,5}).text = "─";
    value_handmade_footer(g,value_handmade_line_model(),23); return g;
}
TEST(MermaidChartsMermanValueOracle, VerticalInsideTokensRetainEverySourceBarAndExactEndpoint) {
    const auto model = value_handmade_model(); const auto good = value_handmade_inside();
    ASSERT_TRUE(merman_value_errors(good,model).empty()) << explain(merman_value_errors(good,model));
    for (const auto bad : {Point{5,15},Point{6,15},Point{5,19}}) {
        auto wrong = good; wrong.edit(bad).text = " "; EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    }
    auto wrong = good; write(wrong,{5,15},"14"); EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; wrong.edit({5,15}).text = " "; wrong.edit({6,15}).text = " "; write(wrong,{6,15},"11");
    EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; for (int y = 16; y < 20; ++y) wrong.edit({5,y}).text = " ";
    EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; wrong.edit({7,18}).text = "█"; EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; write(wrong,{0,24},"Bar 1 A=11"); EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    auto large = model; large.y_first = 4000; large.y_last = 12000; large.authored_y = {{4000,12000}};
    large.ownership.series[0].values = {5000,12000}; large.points = {{{0,5000},{1,12000}}};
    auto scene = value_handmade_resize(merman_handmade_fixed_bars(),64,32);
    for (int y = 0; y <= 20; ++y) for (int x = 0; x < 5; ++x) scene.edit({x,y}).text = " ";
    for (int y = 0; y <= 20; ++y) scene.edit({5,y}).text = "│";
    for (int x = 5; x <= 17; ++x) scene.edit({x,20}).text = "─";
    for (int n = 0; n < 5; ++n) { write(scene,{0,20-n*5},std::to_string(4000+2000*n)); scene.edit({5,20-n*5}).text = n ? "┤" : "┼"; }
    for (int y = 0; y < 20; ++y) for (const int x : {8,14}) scene.edit({x,y}).text = x == 14 || y >= 18 ? "█" : " ";
    for (const int x : {11}) for (int y = 0; y < 20; ++y) scene.edit({x,y}).text = " ";
    scene.edit({8,20}).text = "┬"; scene.edit({14,20}).text = "┬"; scene.edit({11,20}).text = "─";
    for (int x = 0; x < scene.width; ++x) scene.edit({x,21}).text = " ";
    write(scene,{8,21},"A"); write(scene,{14,21},"B"); write(scene,{8,18},"5000"); write(scene,{14,0},"12000");
    ASSERT_TRUE(merman_value_errors(scene,large).empty()) << explain(merman_value_errors(scene,large));
}
TEST(MermaidChartsMermanValueOracle, HorizontalInsideFitAndOutsideFallbackNeverConsumeAllInk) {
    const auto model = value_handmade_model(true); const auto good = value_handmade_inside(true);
    ASSERT_TRUE(merman_value_errors(good,model).empty()) << explain(merman_value_errors(good,model));
    auto wrong = good; write(wrong,{14,2},"14"); EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; wrong.edit({13,2}).text = " "; EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; for (int x = 6; x <= 13; ++x) wrong.edit({x,2}).text = " ";
    EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    auto tiny = model; tiny.ownership.series[0].values[0] = 10.2; tiny.points[0][0].value = 10.2;
    auto fallback = good; for (int x = 6; x <= 15; ++x) fallback.edit({x,2}).text = x <= 7 ? "█" : " ";
    write(fallback,{47,2},"10.2");
    ASSERT_TRUE(merman_value_errors(fallback,tiny).empty()) << explain(merman_value_errors(fallback,tiny));
    auto erased = fallback; erased.edit({6,2}).text = " "; erased.edit({7,2}).text = " ";
    EXPECT_FALSE(merman_value_errors(erased,tiny).empty());
}
TEST(MermaidChartsMermanValueOracle, OutsideRowsCannotBorrowChartOrAxisCaptionsOrDifferentCategory) {
    for (const bool horizontal : {false,true}) {
        auto model = value_handmade_model(horizontal); model.outside_values = true;
        auto raw = value_handmade_resize(horizontal ? merman_handmade_positive_horizontal() : merman_handmade_positive_vertical(),64,32);
        auto good = horizontal ? raw : value_handmade_shift(raw,3);
        if (horizontal) { write(good,{47,2},"11"); write(good,{47,6},"14"); }
        else { write(good,{5,2},"11"); write(good,{11,2},"14"); }
        ASSERT_TRUE(merman_value_errors(good,model).empty()) << explain(merman_value_errors(good,model));
        auto wrong = good; const Point label = horizontal ? Point{47,2} : Point{5,2}; write(wrong,label,"14");
        EXPECT_FALSE(merman_value_errors(wrong,model).empty());
        wrong = good; wrong.edit(label).text = " "; EXPECT_FALSE(merman_value_errors(wrong,model).empty());
        auto inside = model; inside.outside_values = false;
        EXPECT_FALSE(merman_value_errors(good,inside).empty());
        if (!horizontal) {
            auto titled = model; titled.ownership.title = "Chart 11"; titled.ownership.value_title = "Axis 14";
            auto captions = good; write(captions,{0,0},"Chart 11"); write(captions,{0,1},"Axis 14");
            ASSERT_TRUE(merman_value_errors(captions,titled).empty()) << explain(merman_value_errors(captions,titled));
            auto stolen = captions; for (int x = 0; x < stolen.width; ++x) stolen.edit({x,2}).text = " ";
            EXPECT_FALSE(merman_value_errors(stolen,titled).empty());
            stolen = captions; for (int x = 0; x < stolen.width; ++x) stolen.edit({x,1}).text = " ";
            write(stolen,{0,1},"11"); EXPECT_FALSE(merman_value_errors(stolen,titled).empty());
        }
    }
}
TEST(MermaidChartsMermanValueOracle, OrderedFooterNeedsActualLineGeometryAndEveryUniqueSampleRecord) {
    const auto model = value_handmade_line_model(); const auto good = value_handmade_line();
    ASSERT_TRUE(merman_value_errors(good,model).empty()) << explain(merman_value_errors(good,model));
    for (const Point missing : {Point{0,23},Point{8,10},Point{5,15}}) {
        auto wrong = good; wrong.edit(missing).text = " "; EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    }
    auto wrong = good; for (int x = 0; x < wrong.width; ++x) wrong.edit({x,23}).text = " ";
    write(wrong,{0,23},"Line 1 B=1"); EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; write(wrong,{0,26},"Line 1 A=1"); EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; wrong.edit({8,10}).text = "├"; EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; for (int x = 5; x <= 11; ++x) for (int y = 0; y < 20; ++y) wrong.edit({x,y}).text = " ";
    EXPECT_FALSE(merman_value_errors(wrong,model).empty());
}
TEST(MermaidChartsMermanValueOracle, MixedOverlapRetainsAllOwnerContributionsBesidesCompleteValues) {
    auto model = merman_handmade_composite_model(); model.show_values = true;
    auto good = value_handmade_resize(merman_handmade_composite_scene(),96,48);
    for (int row = 27; row < 32; ++row) for (int x = 0; x < good.width; ++x) good.edit({x,row}).text = " ";
    for (const auto& [row,text] : std::vector<std::pair<int,std::string>>{{27,"⠁ Bar 1 + Line 1"},{28,"⠂ Bar 2 + Line 1"},{29,"⠃ Bar 1 + Line 2"},{30,"⠄ Bar 2 + Line 2"}}) write(good,{0,row},text);
    value_handmade_footer(good,model,31);
    ASSERT_TRUE(merman_value_errors(good,model).empty()) << explain(merman_value_errors(good,model));
    auto wrong = good; wrong.edit({11,5}).text = "─"; EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; wrong.edit({8,5}).text = "┬"; EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; for (int x = 0; x < wrong.width; ++x) wrong.edit({x,35}).text = " ";
    EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; write(wrong,{0,39},"Bar 1 A=2"); EXPECT_FALSE(merman_value_errors(wrong,model).empty());
}
TEST(MermaidChartsMermanValueOracle, CanonicalTokensUnicodeAndEqualValuesHaveIndependentCellIdentity) {
    auto model = value_handmade_model(); model.category_names = {"甲","é"};
    auto good = value_handmade_inside(); good.edit({5,21}).text = " "; good.edit({11,21}).text = " ";
    write(good,{5,21},"甲"); write(good,{11,21},"é");
    ASSERT_TRUE(merman_value_errors(good,model).empty()) << explain(merman_value_errors(good,model));
    auto wrong = good; wrong.edit({6,21}).text = "x"; EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    auto near_model = value_handmade_line_model(); near_model.ownership.series[0].values = {1,1.0000000000000002};
    near_model.points = {{{0,1},{1,1.0000000000000002}}};
    auto near = value_handmade_resize(merman_handmade_fixed_bars(),80,36);
    for (int y = 0; y < 20; ++y) { near.edit({5,y}).text = " "; near.edit({11,y}).text = " "; }
    for (int x = 5; x <= 11; ++x) near.edit({x,15}).text = "─";
    value_handmade_footer(near,near_model,23);
    ASSERT_TRUE(merman_value_errors(near,near_model).empty()) << explain(merman_value_errors(near,near_model));
    wrong = near; for (int x = 0; x < wrong.width; ++x) wrong.edit({x,24}).text = " ";
    write(wrong,{0,24},"Line 1 B=1"); EXPECT_FALSE(merman_value_errors(wrong,near_model).empty());
    wrong = near; write(wrong,{0,24},"Line 1 B=1.0000000000000002e1"); EXPECT_FALSE(merman_value_errors(wrong,near_model).empty());
    auto same = near_model; same.ownership.series[0].values = {1,1}; same.points[0][1].value = 1;
    auto same_grid = near; for (int x = 0; x < same_grid.width; ++x) same_grid.edit({x,24}).text = " ";
    write(same_grid,{0,24},"Line 1 B=1");
    ASSERT_TRUE(merman_value_errors(same_grid,same).empty()) << explain(merman_value_errors(same_grid,same));
    for (int x = 0; x < same_grid.width; ++x) same_grid.edit({x,24}).text = " ";
    EXPECT_FALSE(merman_value_errors(same_grid,same).empty());
}
TEST(MermaidChartsMermanValueOracle, TokenMasksAndCandidateScansShareBoundsWithoutDeletingLateInk) {
    const auto model = value_handmade_model(); const auto good = value_handmade_inside();
    Work exhausted{0}; EXPECT_THROW((void)merman_value_errors(good,model,exhausted),std::runtime_error);
    auto wrong = value_handmade_resize(good,240,256); write(wrong,{120,250},"unknown value");
    EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    Work label_work;
    EXPECT_NO_THROW((void)find_text(good,std::string(256,'x'),label_work));
    EXPECT_THROW((void)find_text(good,std::string(257,'x'),label_work),std::runtime_error);
    auto negative = model; negative.y_first = -10; negative.y_last = 10; negative.authored_y = {{-10,10}};
    EXPECT_EQ(merman_value_policy(negative).base.pending,"authored_nonzero_domain_bar_baseline_profile_pending");
    auto zero = model; zero.ownership.series[0].values[0] = 10; zero.points[0][0].value = 10;
    EXPECT_FALSE(merman_value_errors(good,zero).empty());
}
TEST(MermaidChartsSemantics, CompleteMermanValueLabel19SourceUnionAndEligibleEighteenGeometry) {
    const std::set<std::string> expected_ids{
        "merman/0d141f0cc422fa199ee3","merman/1465019ee2ee57a21217","merman/4191b7b5dc6ecbe6b734",
        "merman/4239cdd92c167316194c","merman/59b5415a0a3b8cb73f0d","merman/76934683ae5dc045dbae",
        "merman/83af26d25cc380d2005d","merman/8d26606bfb96f8e5c412","merman/9c890fc9196c5893c9ea",
        "merman/ad4cef3d32e9f763f999","merman/b50974f5288abac7cfe4","merman/b7e2e0438a13f8dff541",
        "merman/c49260ce97f3719a1bdf","merman/d132fb3382109612f2a2","merman/e99c0372aad3ad5c0078",
        "merman/f0ee73a29c41df146077","merman/f50b2db43a0801593459","merman/f5f012ca4e7055701b83",
        "merman/fc1e5eee7ed383d16702"};
    const std::string negative_id = "merman/f5f012ca4e7055701b83";
    json::JsonMutDoc report; auto root = report.object(); auto rows = root.make_arr();
    root.set("kind","complete_source_value_label19_union_with_eighteen_geometry_obligations");
    root.set("source_facts_sha256","eec2dd0522a1fb5818f7bfe8ef17f3f561d3fb30a418443ff39130f81ab76c34");
    root.set("qualification","Source-complete19. Exact original ASCII/API predicates remain pending; labels cannot replace actual data geometry. One independent negative-domain reference dependency stays pending.");
    std::set<std::string> visited; int eligible = 0,pending = 0,source_count = 0,candidates = 0,matches = 0,geometry_errors = 0,oracle_errors = 0;
    int cfg = 0,values = 0,raw_values = 0,inside = 0,outside = 0,disclosure = 0,vertical = 0,horizontal = 0;
    int active_source = 0,active_oracle_errors = 0,observation_errors = 0;
    ASSERT_NO_THROW(merman_xy102_native_batch([&](JsonVal row,JsonVal fact,const MermanXY& model) {
        if (!model.show_values) return;
        const auto id = row.get_string("id"); visited.insert(id); SCOPED_TRACE(id);
        const auto policy = merman_value_policy(model); const bool active = policy.base.pending.empty();
        EXPECT_EQ(active,id != negative_id); if (active) ++eligible; else ++pending;
        horizontal += model.ownership.horizontal; vertical += !model.ownership.horizontal;
        if (!policy.compact) ++disclosure; else if (policy.outside) ++outside; else ++inside;
        cfg += static_cast<int>(fact.get("authored_config_fields").size());
        for (const auto& owner : model.points) for (const auto& point : owner) values += point.value.has_value();
        int tails = 0; fact.get("source_model_data_transformations").iter([&](JsonVal transform) { tails += static_cast<int>(transform.get("omitted_authored_tail").size()); });
        for (const auto& owner : model.ownership.series) raw_values += static_cast<int>(owner.values.size());
        raw_values += tails;
        auto item = root.make_obj(); item.set("id",id); item.set("input_sha256",fact.get_string("input_sha256"));
        item.set("primary_model_sha256",fact.get("primary_model_reference").get_string("sha256"));
        item.set("eligible_geometry",active); item.set("dependency_pending",policy.base.pending);
        item.set("label_profile",!policy.compact ? "all_series_terminal_value_disclosure" : (policy.outside ? "single_bar_outside" : "single_bar_inside"));
        bool is_source = false,observed = false,oracle_error = false; std::vector<std::string> errors;
        try {
            const auto source = row.get_string("input"); auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
            auto ordinary = loom::ui::render_markdown(fenced(source,"text")); const auto wide = capture(element,240,256);
            is_source = same_source_surface(wide,capture(ordinary,240,256)); observed = true;
            if (!active) errors.push_back("Independent pending value-label source profile: "+policy.base.pending);
            else if (is_source) errors.push_back("Eligible value-label source retained as complete code; geometry unmet");
            else errors = merman_value_errors(wide,model);
            const auto narrow = capture(element,40,256); const bool narrow_source = same_source_surface(narrow,capture(ordinary,40,256));
            if (active && !is_source && errors.empty() && !narrow_source)
                for (const auto& error : merman_value_errors(narrow,model)) errors.push_back("Narrow value-label geometry: "+error);
            const bool restored = capture(element,240,256) == wide;
            const bool sentinel = wide.text().find(kSentinel) != std::string::npos && narrow.text().find(kSentinel) != std::string::npos;
            if (!restored) errors.push_back("Value-label width restore changed the complete surface");
            if (!sentinel) errors.push_back("Value-label capture lost following content");
            EXPECT_TRUE(restored); EXPECT_TRUE(sentinel); item.set("restored_identically",restored); item.set("following_content_retained",sentinel);
        } catch (const std::exception& error) { oracle_error = true; errors.push_back("Explicit value-label oracle error: "+std::string(error.what())); }
        if (!observed) ++observation_errors; else if (is_source) ++source_count; else ++candidates;
        if (oracle_error) { ++oracle_errors; active_oracle_errors += active; ADD_FAILURE() << explain(errors); }
        else if (active) { if (is_source) ++active_source; else if (errors.empty()) ++matches; else ++geometry_errors; EXPECT_TRUE(errors.empty()) << explain(errors); }
        item.set("observed",!observed ? "observation_error" : (is_source ? "source_fallback" : "candidate_art"));
        item.set("outcome",oracle_error ? "oracle_error" : (!active ? "dependency_pending" : (is_source ? "source_fallback" : (errors.empty() ? "geometry_match_pending_review" : "geometry_error"))));
        auto failures = root.make_arr(); for (const auto& error : errors) failures.append(root.make_str(error)); item.add("errors",failures); rows.append(item);
    }));
    EXPECT_EQ(visited,expected_ids); EXPECT_EQ(eligible,18); EXPECT_EQ(pending,1); EXPECT_EQ(cfg,51); EXPECT_EQ(values,122); EXPECT_EQ(raw_values,139);
    EXPECT_EQ(inside,13); EXPECT_EQ(outside,2); EXPECT_EQ(disclosure,4); EXPECT_EQ(vertical,9); EXPECT_EQ(horizontal,10);
    EXPECT_EQ(source_count+candidates+observation_errors,19);
    EXPECT_EQ(matches+active_source+geometry_errors+active_oracle_errors,18);
    root.set("complete_source_inputs",static_cast<int>(visited.size())); root.set("eligible_geometry_inputs",eligible); root.set("independent_dependency_inputs_pending",pending);
    root.set("single_bar_inside",inside); root.set("single_bar_outside",outside); root.set("all_series_terminal_value_disclosure",disclosure);
    root.set("authored_config_leaf_applications",cfg); root.set("projected_nonnull_data",values); root.set("raw_authored_values",raw_values);
    root.set("geometry_matches_pending_review",matches); root.set("source_fallback_observations",source_count); root.set("candidate_art_observations",candidates);
    root.set("eligible_source_fallback",active_source); root.set("observation_errors",observation_errors);
    root.set("geometry_errors",geometry_errors); root.set("oracle_errors",oracle_errors); root.set("eligible_oracle_errors",active_oracle_errors); root.set("certified_complete_geometry",0);
    root.set("original_cohort_function_applications_pending",19); root.set("original_global_function_applications_pending",108);
    root.add("cases",rows); report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto path = std::filesystem::path(LOOM_MERMAID_REPORT_DIR)/"mermaid-merman-value-label19-semantics.json";
    std::ofstream out(path,std::ios::binary); ASSERT_TRUE(out && (out << report.to_pretty_string() << '\n')) << "Cannot write " << path;
#endif
}
} // namespace

namespace {
MermanXY value_handmade_identical_twelve_model() {
    // Independently transcribed source 4191b7...: same twelve monthly bar/line
    // values, two distinct graphical owners, exact title and value-axis title.
    MermanXY model; model.show_values = true; model.y_first = 4000; model.y_last = 11000;
    model.authored_y = {{4000,11000}}; model.ownership.title = "Sales Revenue";
    model.ownership.value_title = "Revenue (in $)";
    model.category_names = {"jan","feb","mar","apr","may","jun","jul","aug","sep","oct","nov","dec"};
    const std::vector<double> values{5000,6000,7500,8200,9500,10500,11000,10200,9200,8500,7000,6000};
    model.ownership.series = {{"bar","Bar 1",values},{"line","Line 1",values}};
    for (int owner = 0; owner < 2; ++owner) {
        std::vector<MermanPoint> points;
        for (std::size_t n = 0; n < values.size(); ++n) points.push_back({static_cast<double>(n),values[n]});
        model.points.push_back(std::move(points));
    }
    model.named_series = {false,false}; return model;
}
Grid value_handmade_identical_twelve() {
    // These literal physical endpoint coordinates are independently hand-drawn
    // from domain4000..11000, capacity20 and source-fixed monthly columns.
    // No renderer output or layout function supplies this witness.
    constexpr std::array<Point,12> samples{{{9,19},{15,16},{21,12},{27,10},{33,6},{39,3},
        {45,2},{51,4},{57,7},{63,9},{69,13},{75,16}}};
    Grid grid{112,58,std::vector<Cell>(112u*58u)};
    write(grid,{0,0},"Sales Revenue"); write(grid,{0,1},"Revenue (in $)");
    for (int y = 2; y <= 22; ++y) grid.edit({6,y}).text = "│";
    for (int x = 6; x <= 78; ++x) grid.edit({x,22}).text = "─";
    for (int n = 0; n < 5; ++n) {
        write(grid,{0,22-5*n},std::to_string(4000+1750*n)); grid.edit({6,22-5*n}).text = n ? "┤" : "┼";
    }
    const auto model = value_handmade_identical_twelve_model();
    for (std::size_t n = 0; n < samples.size(); ++n) {
        for (int y = samples[n].y; y < 22; ++y) grid.edit({samples[n].x,y}).text = "█";
        grid.edit({samples[n].x,22}).text = "┬"; write(grid,{samples[n].x-1,23},model.category_names[n]);
    }
    std::vector<int> bits(grid.cells.size());
    auto segment = [&](Point first,Point last) {
        const Point step{(last.x > first.x)-(last.x < first.x),(last.y > first.y)-(last.y < first.y)};
        const int forward = step.x ? R : step.y > 0 ? D : U;
        const int reverse = step.x ? L : step.y > 0 ? U : D;
        for (Point at = first;; at = {at.x+step.x,at.y+step.y}) {
            auto& mask = bits[static_cast<std::size_t>(at.y*grid.width+at.x)];
            if (!(at == first)) mask |= reverse;
            if (!(at == last)) mask |= forward;
            if (at == last) break;
        }
    };
    for (std::size_t n = 1; n < samples.size(); ++n) {
        const auto a = samples[n-1],b = samples[n]; const int middle = a.x+3;
        segment(a,{middle,a.y}); segment({middle,a.y},{middle,b.y}); segment({middle,b.y},b);
    }
    for (std::size_t n = 0; n < bits.size(); ++n) if (bits[n]) {
        if (grid.cells[n].text == "█") { grid.cells[n].text = "⊕"; continue; }
        const int mask = bits[n];
        if (mask == L || mask == R || mask == (L|R)) grid.cells[n].text = "─";
        else if (mask == (U|D)) grid.cells[n].text = "│";
        else if (mask == (R|D)) grid.cells[n].text = "┌";
        else if (mask == (L|D)) grid.cells[n].text = "┐";
        else if (mask == (U|R)) grid.cells[n].text = "└";
        else if (mask == (U|L)) grid.cells[n].text = "┘";
        else throw std::runtime_error("Handwritten twelve-sample witness has an undeclared physical bend");
    }
    write(grid,{0,25},"█ Bar 1"); write(grid,{0,26},"─ Line 1"); write(grid,{0,27},"⊕ Bar 1 + Line 1");
    value_handmade_footer(grid,model,28); return grid;
}
TEST(MermaidChartsMermanValueOracle, IdenticalTwelveSamplesStillNeedTwoOwnersAndTwentyFourRecords) {
    const auto model = value_handmade_identical_twelve_model(); const auto good = value_handmade_identical_twelve();
    int bound = 0;
    ASSERT_NO_THROW(merman_xy102_native_batch([&](JsonVal row,JsonVal fact,const MermanXY& original) {
        if (row.get_string("id") != "merman/4191b7b5dc6ecbe6b734") return;
        ++bound; EXPECT_EQ(fact.get_string("input_sha256"),"4191b7b5dc6ecbe6b73464417555919ff9b13c4c17be733fe9032b25f572fd6b");
        EXPECT_EQ(original.category_names,model.category_names); EXPECT_EQ(original.ownership.title,model.ownership.title);
        EXPECT_EQ(original.ownership.value_title,model.ownership.value_title); EXPECT_EQ(original.authored_y,model.authored_y);
        ASSERT_EQ(original.ownership.series.size(),2u);
        for (std::size_t owner = 0; owner < 2; ++owner) {
            EXPECT_EQ(original.ownership.series[owner].name,model.ownership.series[owner].name);
            EXPECT_EQ(original.ownership.series[owner].type,model.ownership.series[owner].type);
            EXPECT_EQ(original.ownership.series[owner].values,model.ownership.series[owner].values);
        }
    }));
    EXPECT_EQ(bound,1);
    ASSERT_TRUE(merman_value_errors(good,model).empty()) << explain(merman_value_errors(good,model));
    for (int row = 28; row < 52; ++row) {
        auto wrong = good; for (int x = 0; x < wrong.width; ++x) wrong.edit({x,row}).text = " ";
        EXPECT_FALSE(merman_value_errors(wrong,model).empty()) << "Missing independent record " << row;
    }
    for (const Point missing : {Point{0,26},Point{9,19},Point{12,19}}) {
        auto wrong = good; wrong.edit(missing).text = " "; EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    }
    auto wrong = good; wrong.edit({9,19}).text = "─";
    EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; for (int x = 0; x < wrong.width; ++x) wrong.edit({x,40}).text = " ";
    write(wrong,{0,40},"Bar 1 jan=5000"); EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; write(wrong,{0,28},"Bar 1 feb=5000"); EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; write(wrong,{0,28},"Bar 1 jan=5001"); EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; write(wrong,{0,52},"Bar 1 jan=5000"); EXPECT_FALSE(merman_value_errors(wrong,model).empty());
    wrong = good; for (int y = 2; y < 22; ++y) for (int x = 7; x < 79; ++x) wrong.edit({x,y}).text = " ";
    EXPECT_FALSE(merman_value_errors(wrong,model).empty());
}
TEST(MermaidChartsMermanValueOracle, NewTokenMaskDistinguishesLabelCapFromCanvasCapAndCellReuse) {
    auto grid = value_handmade_resize(value_handmade_line(),240,40);
    auto model = value_handmade_line_model().ownership; model.categories = {"A","B"};
    const Axes axis{2,0,20,14}; const Scale calibrated{20,0,0,4};
    const std::vector<int> ports{5,11}; const Legend keys;
    model.series[0].name = std::string(252,'x');
    ASSERT_EQ(ftxui::string_width(value_record(model,0,0)),256);
    std::vector<std::string> errors; Work work; XYValuePlan plan;
    EXPECT_NO_THROW(plan = merman_value_plan(grid,model,axis,calibrated,ports,keys,false,false,work,errors));
    EXPECT_FALSE(errors.empty()); EXPECT_TRUE(plan.bindings.empty());
    // 256 is within the label cap but cannot fit the unchanged 240-cell canvas.
    // 257 must instead fail the actual new token guard before any cell masking.
    model.series[0].name.push_back('x'); ASSERT_EQ(ftxui::string_width(value_record(model,0,0)),257);
    errors.clear(); Work next;
    try { (void)merman_value_plan(grid,model,axis,calibrated,ports,keys,false,false,next,errors); FAIL() << "257-cell value record was admitted"; }
    catch (const std::runtime_error& error) { EXPECT_STREQ(error.what(),"Value label exceeds the unchanged label bound"); }
    const auto collision_grid = value_handmade_inside(); auto collision = value_handmade_model().ownership;
    collision.categories = {"A","B"}; collision.domain = {{10,14}}; collision.series[0].values = {11,11};
    const Scale positive{20,0,10,14}; Work shared; errors.clear();
    const auto collided = merman_value_plan(collision_grid,collision,axis,positive,{5,5},keys,true,false,shared,errors);
    EXPECT_FALSE(errors.empty()); EXPECT_EQ(collided.bindings.size(),1u);
    EXPECT_TRUE(std::ranges::any_of(errors,[](const std::string& error) { return error.find("reused") != std::string::npos; }));
}
} // namespace

namespace {
TEST(MermaidChartsMermanValueOracle, ZeroReferenceCategoryPortsDoNotAcquireForeignAxisArms) {
    // Exact models of pinned0d141/ad4cef: bands A/B, domain0..10, bars4/8,
    // enabled labels, opposite explicit source orientations, no extra fields.
    auto vertical = merman_handmade_rectangular(); vertical.show_values = true;
    vertical.authored_y = {{0,10}}; vertical.y_first = 0; vertical.y_last = 10;
    vertical.ownership.domain = {{0,10}}; vertical.ownership.series[0].values = {4,8}; vertical.points = {{{0,4},{1,8}}};
    auto horizontal = vertical; horizontal.ownership.horizontal = true;
    int source_bindings = 0;
    ASSERT_NO_THROW(merman_xy102_native_batch([&](JsonVal row,JsonVal,const MermanXY& source) {
        if (row.get_string("id") != "merman/0d141f0cc422fa199ee3" && row.get_string("id") != "merman/ad4cef3d32e9f763f999") return;
        ++source_bindings; EXPECT_EQ(source.category_names,vertical.category_names); EXPECT_EQ(source.authored_y,vertical.authored_y);
        ASSERT_EQ(source.ownership.series.size(),1u); EXPECT_EQ(source.ownership.series[0].type,"bar");
        EXPECT_EQ(source.ownership.series[0].values,vertical.ownership.series[0].values); EXPECT_TRUE(source.show_values);
        EXPECT_EQ(source.ownership.horizontal,row.get_string("id") == "merman/ad4cef3d32e9f763f999");
    }));
    EXPECT_EQ(source_bindings,2);
    Grid good{64,32,std::vector<Cell>(64u*32u)};
    for (int y = 0; y <= 20; ++y) good.edit({4,y}).text = "│";
    for (int x = 4; x <= 16; ++x) good.edit({x,20}).text = "─";
    for (int n = 0; n < 5; ++n) { write(good,{0,20-5*n},numeric_x_label(2.5*n)); good.edit({4,20-5*n}).text = n ? "┤" : "┼"; }
    for (const auto& [column,first,label,number] : {std::tuple{7,12,"A","4"},std::tuple{13,4,"B","8"}}) {
        for (int y = first; y < 20; ++y) good.edit({column,y}).text = "█";
        good.edit({column,20}).text = "┬"; write(good,{column,21},label); write(good,{column,first},number);
    }
    ASSERT_TRUE(merman_value_errors(good,vertical).empty()) << explain(merman_value_errors(good,vertical));
    auto extra = good; extra.edit({7,20}).text = "┼";
    EXPECT_FALSE(merman_value_errors(extra,vertical).empty()) << "One zero-reference category port invented an upward arm";
    Grid hgood{64,32,std::vector<Cell>(64u*32u)};
    for (int y = 0; y <= 8; ++y) hgood.edit({5,y}).text = "│";
    for (int x = 5; x <= 45; ++x) hgood.edit({x,8}).text = "─";
    for (int n = 0; n < 5; ++n) {
        const auto text = n == 4 ? "10.00" : numeric_x_label(2.5*n); const int center = 5+10*n;
        write(hgood,{center-(ftxui::string_width(text)-1)/2,9},text); hgood.edit({center,8}).text = n ? "┬" : "┼";
    }
    for (const auto& [row,last,label,number] : {std::tuple{2,21,"A","4"},std::tuple{6,37,"B","8"}}) {
        for (int x = 6; x <= last; ++x) hgood.edit({x,row}).text = "█";
        hgood.edit({5,row}).text = "├"; write(hgood,{3,row},label); write(hgood,{last,row},number);
    }
    ASSERT_TRUE(merman_value_errors(hgood,horizontal).empty()) << explain(merman_value_errors(hgood,horizontal));
    auto hextra = hgood; hextra.edit({5,2}).text = "┼";
    EXPECT_FALSE(merman_value_errors(hextra,horizontal).empty()) << "One zero-reference category port invented a leftward arm";
}
} // namespace

namespace {
TEST(MermaidChartsMermanValueResources, BooleanPrefixesKeepExactTypesDuplicatesAndWholeSourceBoundaries) {
    auto preserves_source = [](std::string_view source) {
        auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
        auto ordinary = loom::ui::render_markdown(fenced(source,"text"));
        const auto wide = capture(element,240,256);
        for (const int width : {240,40,240}) {
            const auto frame = capture(element,width,256);
            EXPECT_TRUE(same_source_surface(frame,capture(ordinary,width,256)));
            EXPECT_NE(frame.text().find(kSentinel),std::string::npos);
            if (width == 240) EXPECT_TRUE(frame == wide);
        }
    };
    auto enabled_geometry = [](const std::string& source,const MermanXY& model) {
        auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
        auto ordinary = loom::ui::render_markdown(fenced(source,"text"));
        const auto wide = capture(element,240,256);
        const bool source_surface = same_source_surface(wide,capture(ordinary,240,256));
        EXPECT_FALSE(source_surface) << "Known enabled boolean source still lacks geometry";
        if (!source_surface) EXPECT_TRUE(merman_value_errors(wide,model).empty()) << explain(merman_value_errors(wide,model));
        for (const int width : {240,40,240}) {
            const auto frame = capture(element,width,256);
            EXPECT_NE(frame.text().find(kSentinel),std::string::npos);
            if (width == 240) EXPECT_TRUE(frame == wide);
            if (width == 40 && !same_source_surface(frame,capture(ordinary,width,256)))
                EXPECT_TRUE(merman_value_errors(frame,model).empty()) << explain(merman_value_errors(frame,model));
        }
    };
    // Boolean false is meaningful: known presentation options must preserve the
    // ordinary chart pixels, rather than being rejected or coercing a label on.
    const std::vector<std::string> valid_false_prefixes{
        "%%{init: {\"xyChart\": {\"showDataLabel\": false}}}%%\n",
        "%%{init: {\"xyChart\": {\"showDataLabel\": false, \"showDataLabelOutsideBar\": true}}}%%\n",
        "---\nconfig:\n  xyChart:\n    showDataLabel: false\n---\n",
        "---\nconfig:\n  xyChart:\n    showDataLabel: false\n    showDataLabelOutsideBar: true\n---\n"};
    const std::vector<std::pair<std::string,bool>> valid_true_prefixes{
        {"%%{init: {\"xyChart\": {\"showDataLabel\": true}}}%%\n",false},
        {"%%{init: {\"xyChart\": {\"showDataLabel\": true, \"showDataLabelOutsideBar\": false}}}%%\n",false},
        {"%%{init: {\"xyChart\": {\"showDataLabel\": true, \"showDataLabelOutsideBar\": true}}}%%\n",true},
        {"---\nconfig:\n  xyChart:\n    showDataLabel: true\n---\n",false},
        {"---\nconfig:\n  xyChart:\n    showDataLabel: true\n    showDataLabelOutsideBar: false\n---\n",false},
        {"---\nconfig:\n  xyChart:\n    showDataLabel: true\n    showDataLabelOutsideBar: true\n---\n",true}};
    for (const bool horizontal : {false,true}) {
        auto model = merman_handmade_rectangular(); model.ownership.horizontal = horizontal;
        const auto source = std::string(horizontal ? "xychart horizontal\n" : "xychart\n")+
            "x-axis [A,B]\ny-axis 0 --> 4\nbar [2,4]\n";
        const auto plain = capture(loom::ui::render_markdown(fenced(source,"mermaid")),240,256);
        for (const auto& prefix : valid_false_prefixes) {
            const auto errors = merman_orientation_source_errors(prefix+source,model);
            EXPECT_TRUE(errors.empty()) << prefix << explain(errors);
            EXPECT_TRUE(capture(loom::ui::render_markdown(fenced(prefix+source,"mermaid")),240,256) == plain) << prefix;
        }
        for (const auto& [prefix,outside] : valid_true_prefixes) {
            auto enabled = model; enabled.show_values = true; enabled.outside_values = outside;
            enabled_geometry(prefix+source,enabled);
        }
    }
    const std::string body = "xychart\nx-axis [A,B]\ny-axis 0 --> 4\nbar [2,4]\n";
    for (const std::string& prefix : std::vector<std::string>{
        "%%{init: {\"xyChart\": {\"showDataLabel\": \"true\"}}}%%\n",
        "%%{init: {\"xyChart\": {\"showDataLabel\": 1}}}%%\n",
        "%%{init: {\"xyChart\": {\"showDataLabel\": null}}}%%\n",
        "%%{init: {\"xyChart\": {\"showDataLabelOutsideBar\": []}}}%%\n",
        "%%{init: {\"xyChart\": {\"showDataLabel\": true, \"showDataLabel\": false}}}%%\n",
        "%%{init: {\"xyChart\": {\"showDataLabel\": true}, \"xyChart\": {\"showDataLabel\": false}}}%%\n",
        "%%{init: {\"xyChart\": {\"showDataLabel\": true, \"showDataLabelOutsideBar\": false, \"showDataLabelOutsideBar\": true}}}%%\n",
        "%%{init: {\"xyChart\": {\"showDataLabel\": true}, \"xy\\u0043hart\": {\"showDataLabel\": false}}}%%\n",
        "%%{init: {\"xyChart\": {\"showDataLabel\": true, \"showData\\u004cabel\": false}}}%%\n",
        "%%{init: {\"xyChart\": {\"showDataLabel\": true, \"unknown\": false}}}%%\n",
        "%%{init: {\"xyChart\": {\"showDataLabel\": true}, \"theme\": \"dark\"}}%%\n",
        "%%{init: {\"xyChart\": {}}}%%\n",
        "%%{init: {\"xyChart\": null}}%%\n",
        "%%{init: []}%%\n",
        "%%{init: {xyChart: {showDataLabel: true}}}%%\n",
        "%%{init: {\"xyChart\": {\"showDataLabel\": true,}}}%%\n",
        "%%{init: {\"xyChart\": {\"showDataLabel\": true}}}%% trailing\n",
        "%%{unknown: {\"xyChart\": {\"showDataLabel\": true}}}%%\n",
        "%%{init: {\"xyChart\": {\"showDataLabel\": true}}}%%\n%%{init: {\"xyChart\": {\"showDataLabel\": false}}}%%\n",
        "%%{init: {\"xyChart\": {\"showDataLabel\": true}}}%%\n---\nconfig:\n  xyChart:\n    width: 200\n---\n",
        "---\nconfig:\n  xyChart:\n    width: 200\n---\n%%{init: {\"xyChart\": {\"showDataLabel\": true}}}%%\n",
        "---\nconfig:\n  xyChart:\n    showDataLabel: \"true\"\n---\n",
        "---\nconfig:\n  xyChart:\n    showDataLabel: 1\n---\n",
        "---\nconfig:\n  xyChart:\n    showDataLabel: true\n    showDataLabel: false\n---\n",
        "---\nconfig:\n  xyChart:\n    showDataLabel: true\n    showDataLabelOutsideBar: true\n    unknown: false\n---\n"}) preserves_source(prefix+body);
    preserves_source(body+"%%{init: {\"xyChart\": {\"showDataLabel\": true}}}%%\n");
    preserves_source("%%{init: {\"xyChart\": {\"showDataLabel\": true}}}%%\npie\n\"A\": 1\n");
    preserves_source("---\nconfig:\n  xyChart:\n    showDataLabel: true\n---\npie showData\n\"A\": 1\n");
    preserves_source("%%{init: {\"pie\": {\"showDataLabel\": true}}}%%\npie\n\"A\": 1\n");
}
} // namespace

namespace {
TEST(MermaidChartsMermanValueResources, NamedCompactValuesKeepAuthoredIdentityUntilAnOwnedKeyIsSupported) {
    // These are valid source/profile dependencies, rather than invalid Mermaid
    // or certified drawings. The paired unnamed source must expose real data.
    for (const bool horizontal : {false,true}) for (const bool outside : {false,true}) {
        const auto prefix = std::string("%%{init: {\"xyChart\": {\"showDataLabel\": true, \"showDataLabelOutsideBar\": ")+
            (outside ? "true" : "false")+"}}}%%\n";
        const auto header = std::string(horizontal ? "xychart horizontal\n" : "xychart\n")+
            "x-axis [A,B]\ny-axis 0 --> 4\n";
        const auto named_source = prefix+header+"bar \"Revenue\" [2,4]\n";
        const auto unnamed_source = prefix+header+"bar [2,4]\n";
        auto named = loom::ui::render_markdown(fenced(named_source,"mermaid"));
        auto named_plain = loom::ui::render_markdown(fenced(named_source,"text"));
        auto unnamed = loom::ui::render_markdown(fenced(unnamed_source,"mermaid"));
        auto unnamed_plain = loom::ui::render_markdown(fenced(unnamed_source,"text"));
        const auto named_wide = capture(named,240,256),unnamed_wide = capture(unnamed,240,256);
        auto model = merman_handmade_rectangular(); model.ownership.horizontal = horizontal;
        model.show_values = true; model.outside_values = outside;
        const bool source = same_source_surface(unnamed_wide,capture(unnamed_plain,240,256));
        EXPECT_FALSE(source) << "The unnamed partner remains an active geometry requirement";
        if (!source) EXPECT_TRUE(merman_value_errors(unnamed_wide,model).empty()) << explain(merman_value_errors(unnamed_wide,model));
        for (const int width : {240,40,240}) {
            const auto named_frame = capture(named,width,256),unnamed_frame = capture(unnamed,width,256);
            EXPECT_TRUE(same_source_surface(named_frame,capture(named_plain,width,256)));
            EXPECT_NE(named_frame.text().find(kSentinel),std::string::npos);
            EXPECT_NE(unnamed_frame.text().find(kSentinel),std::string::npos);
            if (width == 240) { EXPECT_TRUE(named_frame == named_wide); EXPECT_TRUE(unnamed_frame == unnamed_wide); }
            if (width == 40 && !same_source_surface(unnamed_frame,capture(unnamed_plain,width,256)))
                EXPECT_TRUE(merman_value_errors(unnamed_frame,model).empty()) << explain(merman_value_errors(unnamed_frame,model));
        }
    }
}
} // namespace

namespace {
XY generic_arm_pinned_ascending_model() {
    XY result; int found = 0;
    batch([&](const Vendor&,JsonVal row) {
        if (row.get_string("id") != "beautiful_mermaid_0c0734e8d421e2e4") return;
        ++found;
        if (row.get_string("input") != "xychart-beta\n      x-axis [A, B]\n      line [10, 50]")
            throw std::runtime_error("Generic arm witness source changed");
        result = xy_model(row.get("expectation").get("semantic_model"),false);
    });
    if (found != 1 || result.categories != std::vector<std::string>{"A","B"} || result.series.size() != 1 ||
        result.series[0].type != "line" || result.series[0].values != std::vector<double>{10,50} || result.domain)
        throw std::runtime_error("Generic arm witness independent source model changed");
    return result;
}
Grid generic_arm_axis(int width = 20) {
    Grid grid{width,8,std::vector<Cell>(static_cast<std::size_t>(width*8))};
    for (int y = 0; y <= 6; ++y) {
        write(grid,{0,y},y == 6 ? " 0" : std::to_string(60-10*y));
        grid.edit({2,y}).text = "┤";
    }
    for (int x = 2; x <= 12; ++x) grid.edit({x,6}).text = "─";
    grid.edit({2,6}).text = "┼";
    for (const auto& [x,label] : {std::pair{4,"A"},std::pair{10,"B"}}) {
        grid.edit({x,6}).text = "┬"; write(grid,{x,7},label);
    }
    return grid;
}
Grid generic_arm_ascending() {
    auto grid = generic_arm_axis();
    for (int y = 2; y <= 5; ++y) grid.edit({4,y}).text = "│";
    grid.edit({4,1}).text = "╭";
    for (int x = 5; x <= 10; ++x) grid.edit({x,1}).text = "─";
    return grid;
}
TEST(MermaidChartsGenericArmOracle, PinnedSourceAndEverySingleCellForeignDirectionAreIndependent) {
    // Source has no authored domain. This six-row 0..60 calibration is a legal
    // native padded-range witness, not the foreign model's automatic 0..54 rule.
    const auto model = generic_arm_pinned_ascending_model(); const auto good = generic_arm_ascending();
    ASSERT_TRUE(xy_errors(good,model).empty()) << explain(xy_errors(good,model));
    for (const auto& [point,glyph] : std::vector<std::pair<Point,std::string>>{
            {{4,3},"├"},{{4,3},"┤"},{{7,1},"┴"},{{7,1},"┬"}}) {
        auto wrong = good; wrong.edit(point).text = glyph;
        EXPECT_FALSE(xy_errors(wrong,model).empty()) << "Only one necessary glyph acquired a foreign arm: " << glyph;
    }
    // Exercise the old native-role nonshared entry independently. It used to
    // enable typed owners but still omitted the selected-stroke closure.
    auto native = merman_handmade_rectangular().ownership; native.categories = {"A","B"};
    native.series[0].type = "line"; native.series[0].name = "Line 1";
    auto fixed = merman_handmade_fixed_bars();
    for (auto& cell : fixed.cells) if (cell.text == "█") cell.text = " ";
    for (int x = 5; x < 8; ++x) fixed.edit({x,10}).text = "─";
    fixed.edit({8,10}).text = "╯";
    for (int y = 1; y < 10; ++y) fixed.edit({8,y}).text = "│";
    fixed.edit({8,0}).text = "╭";
    for (int x = 9; x <= 11; ++x) fixed.edit({x,0}).text = "─";
    Work valid_work;
    ASSERT_TRUE(xy_candidate_errors(fixed,native,Axes{2,0,20,14},valid_work,true).empty());
    fixed.edit({8,5}).text = "├"; Work invalid_work;
    EXPECT_FALSE(xy_candidate_errors(fixed,native,Axes{2,0,20,14},invalid_work,true).empty());
}
TEST(MermaidChartsGenericArmOracle, CalibratedAxisPortsDoNotBorrowAdjacentCapsOrEmptyCells) {
    const auto model = generic_arm_pinned_ascending_model(); const auto good = generic_arm_ascending();
    ASSERT_TRUE(xy_errors(good,model).empty());
    for (const auto& [point,glyph] : std::vector<std::pair<Point,std::string>>{
            {{2,3},"┼"},{{7,6},"┬"},{{4,6},"┼"}}) {
        auto wrong = good; wrong.edit(point).text = glyph;
        EXPECT_FALSE(xy_errors(wrong,model).empty()) << "Axis port lacks a matching source token/path: " << glyph;
    }
    SCOPED_TRACE("Declared paired source: xychart / x-axis [A,B] / y-axis 0 --> 4 / line [0,0]");
    auto baseline = basic_xy("line"); baseline.series[0].values = {0,0};
    const auto union_scene = scene({"4┤", "3┤", "2┤", "1┤", "0┼─┳━━━━━┳─", "   A     B"});
    ASSERT_TRUE(xy_errors(union_scene,baseline).empty()) << explain(xy_errors(union_scene,baseline));
    auto missing_port = union_scene; missing_port.edit({3,4}).text = "━";
    EXPECT_FALSE(xy_errors(missing_port,baseline).empty()) << "Actual category D port disappeared under data";
    auto bar_union = vertical_bars(); bar_union.edit({3,4}).text = "┼";
    ASSERT_TRUE(xy_errors(bar_union,basic_xy()).empty()) << explain(xy_errors(bar_union,basic_xy()));
    auto no_shaft = bar_union; no_shaft.edit({3,3}).text = " ";
    EXPECT_FALSE(xy_errors(no_shaft,basic_xy()).empty());
}
TEST(MermaidChartsGenericArmOracle, OnlyRealGlobalEndsGetCollinearCapsAndSingletonsRemainPoints) {
    const auto model = generic_arm_pinned_ascending_model(); const auto good = generic_arm_ascending();
    ASSERT_TRUE(xy_errors(good,model).empty());
    auto wrong = good; wrong.edit({4,5}).text = "├";
    EXPECT_FALSE(xy_errors(wrong,model).empty()) << "A true endpoint acquired a perpendicular arm";
    wrong = good; wrong.edit({10,1}).text = "┬";
    EXPECT_FALSE(xy_errors(wrong,model).empty());
    SCOPED_TRACE("Declared three-point source: xychart / x-axis [A,B,C] / y-axis 0 --> 4 / line [3,1,3]");
    XY inner{"","","",false,{{0,4}},{"A","B","C"},{{"line","Line 1",{3,1,3}}}};
    const auto internal = scene({"4┤", "3┤ ──────┬──────", "2┤       │", "1┤       ●", "0┼─┬─────┬─────┬─", "   A     B     C"});
    ASSERT_TRUE(xy_errors(internal,inner).empty()) << explain(xy_errors(internal,inner));
    auto false_cap = internal; false_cap.edit({9,3}).text = "│";
    EXPECT_FALSE(xy_errors(false_cap,inner).empty()) << "An internal sample cannot borrow a global straight cap";
    auto one = basic_xy("line"); one.categories = {"A"}; one.series[0].values = {2};
    const auto singleton = scene({"4┤", "3┤", "2┤ ─", "1┤", "0┼─┬───────", "   A"});
    EXPECT_TRUE(xy_errors(singleton,one).empty());
    auto crossed_point = singleton; crossed_point.edit({3,2}).text = "┼";
    EXPECT_FALSE(xy_errors(crossed_point,one).empty()) << "A structural cross is not the declared singleton point glyph";
}
TEST(MermaidChartsGenericArmOracle, CompleteDetoursAndBacktrackingFailWithoutUsingDanglingArms) {
    SCOPED_TRACE("Declared flat source: xychart / x-axis [A,B] / y-axis 0 --> 4 / line [2,2]");
    auto flat = basic_xy("line"); flat.series[0].values = {2,2};
    const auto genuine = scene({"4┤", "3┤", "2┤ ───────", "1┤", "0┼─┬─────┬─", "   A     B"});
    ASSERT_TRUE(xy_errors(genuine,flat).empty());
    const auto detour = scene({"4┤", "3┤ ╭─────╮", "2┤ │     │", "1┤", "0┼─┬─────┬─", "   A     B"});
    EXPECT_FALSE(xy_errors(detour,flat).empty()) << "Every detour glyph is connected, but the flat source never peaks";
    const auto ascending = generic_arm_pinned_ascending_model(); auto backtrack = generic_arm_axis();
    for (int x = 4; x < 8; ++x) backtrack.edit({x,5}).text = "─";
    backtrack.edit({8,5}).text = "╯"; backtrack.edit({8,4}).text = "│";
    backtrack.edit({8,3}).text = "╮"; backtrack.edit({7,3}).text = "─";
    backtrack.edit({6,3}).text = "╰"; backtrack.edit({6,2}).text = "│";
    backtrack.edit({6,1}).text = "╭";
    for (int x = 7; x <= 10; ++x) backtrack.edit({x,1}).text = "─";
    EXPECT_FALSE(xy_errors(backtrack,ascending).empty()) << "A complete path must not move backward along source categories";
    auto max_line = basic_xy("line"); max_line.series[0].values = {4,4};
    const auto plain_frame = scene({"4┬─────────┐", "3┤         │", "2┤         │", "1┤         │", "0┼─┬─────┬─┘", "   A     B"});
    EXPECT_FALSE(xy_errors(plain_frame,max_line).empty());
}
TEST(MermaidChartsGenericArmOracle, LiteralCompositeAndColorKeysStillNeedTheCorrectSourceRoutes) {
    SCOPED_TRACE("Declared coincident source: xychart / x-axis [A,B] / y-axis 0 --> 4 / three line [2,2] owners");
    auto coincident = basic_xy("line");
    coincident.series = {{"line","Line 1",{2,2}},{"line","Line 2",{2,2}},{"line","Line 3",{2,2}}};
    const auto keyed = scene({"4┤", "3┤", "2┤ ⊕⊕⊕⊕⊕⊕⊕", "1┤", "0┼─┬─────┬─", "   A     B", "", "─ Line 1", "━ Line 2", "┄ Line 3", "⊕ Line 1 + Line 2 + Line 3"});
    ASSERT_TRUE(xy_errors(keyed,coincident).empty());
    auto wrong_key = keyed;
    for (int x = 0; x < wrong_key.width; ++x) wrong_key.edit({x,10}).text = " ";
    write(wrong_key,{0,10},"⊕ Line 2 + Line 3");
    EXPECT_FALSE(xy_errors(wrong_key,coincident).empty());
    auto structural = keyed; structural.edit({6,2}).text = "┼";
    EXPECT_FALSE(xy_errors(structural,coincident).empty());
    auto mixed = basic_xy("line"); mixed.series = {{"line","Line 1",{1,1}},{"line","Line 2",{3,3}}};
    auto colored = scene({"4┤", "3┤ ───────", "2┤", "1┤ ───────", "0┼─┬─────┬─", "   A     B", "", "─ Line 1", "─ Line 2"});
    for (int x = 3; x <= 9; ++x) { colored.edit({x,3}).foreground = "red"; colored.edit({x,1}).foreground = "blue"; }
    colored.edit({0,7}).foreground = "red"; colored.edit({0,8}).foreground = "blue";
    ASSERT_TRUE(xy_errors(colored,mixed).empty());
    auto borrowed = scene({"4┤", "3┤ ⊕⊕⊕⊕⊕⊕⊕", "2┤ │     │", "1┤ │     │", "0┼─┬─────┬─", "   A     B", "", "─ Line 1", "─ Line 2", "⊕ Line 1 + Line 2"});
    for (const int x : {3,9}) for (const int y : {2,3}) borrowed.edit({x,y}).foreground = "red";
    borrowed.edit({0,7}).foreground = "red"; borrowed.edit({0,8}).foreground = "blue";
    EXPECT_FALSE(xy_errors(borrowed,mixed).empty()) << "A literal shared key cannot lend a neighbor's flat route to another value";
}
TEST(MermaidChartsGenericArmOracle, AtomicBarMarksKeepTheirExactSourcePointShaftAndDisclosure) {
    SCOPED_TRACE("Declared automatic-range source: xychart / x-axis [A,B] / bar [-4,4]; independent visible range -4..4");
    auto model = basic_xy(); model.domain = {{-4,4}}; model.series[0].values = {-4,4}; model.single_bar_axis_marker = true;
    Grid negative{14,6,std::vector<Cell>(84)};
    for (int y = 0; y <= 4; ++y) {
        const auto token = std::to_string(4-2*y); write(negative,{2-static_cast<int>(token.size()),y},token);
        negative.edit({2,y}).text = "┤";
    }
    for (int x = 2; x <= 12; ++x) negative.edit({x,4}).text = "─";
    negative.edit({2,4}).text = "┼"; negative.edit({4,4}).text = "⊕"; negative.edit({10,4}).text = "┬";
    write(negative,{4,5},"A"); write(negative,{10,5},"B"); negative.edit({4,3}).text = "█";
    negative.edit({10,0}).text = "█"; negative.edit({10,1}).text = "█";
    ASSERT_TRUE(xy_errors(negative,model).empty()) << explain(xy_errors(negative,model));
    auto lost = negative; lost.edit({4,3}).text = " "; EXPECT_FALSE(xy_errors(lost,model).empty());
    auto extra = negative; extra.edit({7,4}).text = "⊕";
    EXPECT_FALSE(xy_errors(extra,model).empty()) << "An unselected empty-axis symbol is not a source endpoint";
    auto middle = negative; middle.edit({4,3}).text = "⊕";
    EXPECT_FALSE(xy_errors(middle,model).empty()) << "The endpoint symbol cannot replace an ordinary shaft cell";
    auto tiny = basic_xy(); tiny.series[0].values = {0.1,0.1};
    const auto points = scene({"4┤", "3┤", "2┤", "1┤", "0┼─▁─────▁─", "   A     B", "", "Bar 1 A=0.1 B=0.1"});
    ASSERT_TRUE(xy_errors(points,tiny).empty());
    auto no_value = points; for (int x = 0; x < no_value.width; ++x) no_value.edit({x,7}).text = " ";
    EXPECT_FALSE(xy_errors(no_value,tiny).empty());
    auto zeros = tiny; zeros.series[0].values = {0,0}; EXPECT_FALSE(xy_errors(points,zeros).empty());
}
TEST(MermaidChartsGenericArmOracle, BoundsAndWorkAreSharedAcrossOrdinaryAndTypedPaths) {
    const auto source = generic_arm_pinned_ascending_model(); const auto grid = generic_arm_ascending();
    Work exhausted; exhausted.left = 1;
    EXPECT_THROW(xy_candidate_errors(grid,source,Axes{2,0,6,12},exhausted),std::runtime_error);
    auto late = grid; late.edit({19,7}).text = "─";
    EXPECT_FALSE(xy_errors(late,source).empty());
    const Grid oversized{241,1,std::vector<Cell>(241)};
    EXPECT_THROW(xy_errors(oversized,source),std::runtime_error);
    auto native = merman_handmade_rectangular().ownership; native.categories = {"A","B"};
    Work one; one.left = 1;
    EXPECT_THROW(xy_candidate_errors(merman_handmade_fixed_bars(),native,Axes{2,0,20,14},one,true),std::runtime_error);
}
} // namespace

namespace {
MermanXY named_singleton_handmade_model(std::string type = "bar",bool horizontal = false,std::string name = "Revenue") {
    // Declared source: xychart[ horizontal] / x-axis [A,B] /
    // y-axis 0 --> 4 / bar|line "Revenue" [2,4].
    auto model = merman_handmade_rectangular(); model.ownership.horizontal = horizontal;
    model.ownership.series[0].type = std::move(type); model.ownership.series[0].name = std::move(name);
    model.named_series[0] = true; return model;
}
Grid named_singleton_handmade(const MermanXY& model) {
    // Independent source-fixed primitives, not a renderer-output snapshot. The
    // actual assertions still recover/verify every port, tick, sample and edge.
    const auto& m = model.ownership; const bool h = m.horizontal;
    const auto& top_title = h ? m.category_title : m.value_title;
    const auto& bottom_title = h ? m.value_title : m.category_title;
    const int top = static_cast<int>(!m.title.empty())+static_cast<int>(!top_title.empty());
    std::array<std::string,5> ticks;
    int tick_width = 1;
    for (int n = 0; n < 5; ++n) {
        ticks[static_cast<std::size_t>(n)] = numeric_x_label(model.y_first+(model.y_last-model.y_first)*n/4);
        tick_width = std::max(tick_width,ftxui::string_width(ticks[static_cast<std::size_t>(n)]));
    }
    const int left = h ? 4 : tick_width+1;
    const auto& plot = m.series.front(); const int count = static_cast<int>(plot.values.size());
    int label_width = 0;
    const auto labels = [&] {
        if (model.band) return model.category_names;
        std::vector<std::string> result;
        for (const auto& point : model.points.front()) result.push_back(numeric_x_label(point.x));
        return result;
    }();
    for (const auto& label : labels) label_width = std::max(label_width,ftxui::string_width(label));
    const int stride = h ? 4 : std::max(6,label_width+3);
    const int bottom = h ? top+2+(count-1)*stride+2 : top+20;
    const int right = h ? left+40 : left+stride/2+(count-1)*stride+stride/2;
    const int key_row = bottom+3+static_cast<int>(!bottom_title.empty());
    const int width = std::max({right+4,ftxui::string_width(plot.name)+3,ftxui::string_width(m.title)+1,
        ftxui::string_width(top_title)+1,ftxui::string_width(bottom_title)+1});
    Grid grid{width,key_row+2,std::vector<Cell>(static_cast<std::size_t>(width*(key_row+2)))};
    if (!m.title.empty()) write(grid,{0,0},m.title);
    if (!top_title.empty()) write(grid,{0,m.title.empty() ? 0 : 1},top_title);
    for (int y = top; y <= bottom; ++y) grid.edit({left,y}).text = "│";
    for (int x = left; x <= right; ++x) grid.edit({x,bottom}).text = "─";
    for (int n = 0; n < 5; ++n) {
        const auto& token = ticks[static_cast<std::size_t>(n)];
        if (h) { const int x = left+10*n; grid.edit({x,bottom}).text = "┬"; write(grid,{x-(ftxui::string_width(token)-1)/2,bottom+1},token); }
        else { const int y = bottom-5*n; grid.edit({left,y}).text = "┤"; write(grid,{left-1-ftxui::string_width(token),y},token); }
    }
    grid.edit({left,bottom}).text = "┼";
    std::vector<Point> points;
    const double reference = model.y_first > 0 ? model.y_first : 0;
    const auto project = [&](double value) { const double unit = (value-model.y_first)/(model.y_last-model.y_first);
        return h ? left+static_cast<int>(std::round(40*unit)) : bottom-static_cast<int>(std::round(20*unit)); };
    for (int n = 0; n < count; ++n) {
        const int slot = h ? top+2+n*stride : left+stride/2+n*stride;
        const auto& label = labels[static_cast<std::size_t>(n)];
        if (h) { grid.edit({left,slot}).text = "├"; write(grid,{left-1-ftxui::string_width(label),slot},label); }
        else { grid.edit({slot,bottom}).text = "┬"; write(grid,{slot-(ftxui::string_width(label)-1)/2,bottom+1},label); }
        const int value = project(plot.values[static_cast<std::size_t>(n)]);
        points.push_back(h ? Point{value,slot} : Point{slot,value});
        if (plot.type == "bar") {
            const int baseline = project(reference),step = value > baseline ? 1 : -1;
            if (value != baseline) for (int p = baseline+step; p != value+step; p += step) {
                const Point at = h ? Point{p,slot} : Point{slot,p};
                grid.edit(at).text = !h && p == bottom && plot.values[static_cast<std::size_t>(n)] < 0 ? "⊕" : "█";
            }
        }
    }
    if (plot.type == "line") {
        std::vector<int> used(grid.cells.size());
        auto segment = [&](Point first,Point last) {
            const Point step{(last.x > first.x)-(last.x < first.x),(last.y > first.y)-(last.y < first.y)};
            const int forward = step.x > 0 ? R : step.x < 0 ? L : step.y > 0 ? D : U;
            const int reverse = step.x > 0 ? L : step.x < 0 ? R : step.y > 0 ? U : D;
            for (Point p = first;; p = {p.x+step.x,p.y+step.y}) {
                auto& bits = used[static_cast<std::size_t>(p.y*width+p.x)];
                if (!(p == first)) bits |= reverse; if (!(p == last)) bits |= forward;
                if (p == last) break;
            }
        };
        for (std::size_t n = 1; n < points.size(); ++n) {
            const auto a = points[n-1],b = points[n]; const int middle = h ? (a.y+b.y)/2 : (a.x+b.x)/2;
            const Point one = h ? Point{a.x,middle} : Point{middle,a.y};
            const Point two = h ? Point{b.x,middle} : Point{middle,b.y};
            segment(a,one); segment(one,two); segment(two,b);
        }
        constexpr std::array<std::pair<int,std::string_view>,11> glyphs{{{U|D,"│"},{L|R,"─"},{R|D,"╭"},{L|D,"╮"},
            {U|R,"╰"},{U|L,"╯"},{U|D|R,"├"},{U|D|L,"┤"},{L|R|D,"┬"},{L|R|U,"┴"},{U|R|D|L,"┼"}}};
        for (std::size_t at = 0; at < used.size(); ++at) {
            int bits = used[at]; if (!bits) continue;
            if (bits == U || bits == D) bits = U|D;
            if (bits == L || bits == R) bits = L|R;
            for (const auto& [expected,glyph] : glyphs) if (expected == bits) grid.cells[at].text = glyph;
        }
    }
    if (!bottom_title.empty()) write(grid,{0,bottom+2},bottom_title);
    write(grid,{0,key_row},(plot.type == "bar" ? "█ " : "─ ")+plot.name);
    bounded(grid); return grid;
}
TEST(MermaidChartsNamedSingletonOracle, CompleteOwnerKeysNeverReplaceActualBarsOrSelectedLines) {
    for (const bool horizontal : {false,true}) for (const std::string& type : {std::string("bar"),std::string("line")}) {
        SCOPED_TRACE(type+(horizontal ? " horizontal" : " vertical"));
        const auto model = named_singleton_handmade_model(type,horizontal); const auto good = named_singleton_handmade(model);
        ASSERT_TRUE(merman_rectangular_errors(good,model,true).empty()) << explain(merman_rectangular_errors(good,model,true));
        const int footer = good.height-2;
        auto wrong = good; for (int x = 0; x < good.width; ++x) wrong.edit({x,footer}).text = " ";
        EXPECT_FALSE(merman_rectangular_errors(wrong,model,true).empty());
        wrong = good; wrong.edit({2,footer}).text = "X"; EXPECT_FALSE(merman_rectangular_errors(wrong,model,true).empty());
        wrong = good; wrong.edit({0,footer}).text = type == "bar" ? "─" : "█";
        EXPECT_FALSE(merman_rectangular_errors(wrong,model,true).empty());
        wrong = good;
        const Point data = horizontal ? (type == "bar" ? Point{5,2} : Point{24,4}) : (type == "bar" ? Point{5,15} : Point{8,5});
        wrong.edit(data).text = " "; EXPECT_FALSE(merman_rectangular_errors(wrong,model,true).empty());
        auto table = scene({"A=2 B=4","█ Revenue"}); EXPECT_FALSE(merman_rectangular_errors(table,model,true).empty());
        wrong = good; write(wrong,{0,footer+1},"Revenue A=2");
        EXPECT_FALSE(merman_rectangular_errors(wrong,model,true).empty()) << "Correct but undeclared extra readout has no named-profile role";
        if (type == "line") for (const std::string& glyph : std::vector<std::string>{"│","┼","╭"}) {
            wrong = good; wrong.edit({0,footer}).text = glyph;
            EXPECT_FALSE(merman_rectangular_errors(wrong,model,true).empty()) << "A structural line junction is not a horizontal owner key";
        }
    }
}
TEST(MermaidChartsNamedSingletonOracle, SameAuthoredCaptionTextNeedsSeparateFooterCellsAndCorrectStyle) {
    auto model = named_singleton_handmade_model(); model.ownership.title = "█ Revenue";
    model.ownership.category_title = "█ Revenue"; model.ownership.value_title = "Value";
    const auto good = named_singleton_handmade(model);
    ASSERT_TRUE(merman_rectangular_errors(good,model,true).empty()) << explain(merman_rectangular_errors(good,model,true));
    const int footer = good.height-2;
    auto wrong = good; for (int x = 0; x < good.width; ++x) wrong.edit({x,footer}).text = " ";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model,true).empty()) << "Same caption text is not a footer key";
    wrong = good; for (int x = 0; x < good.width; ++x) wrong.edit({x,0}).text = " ";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model,true).empty());
    wrong = good; wrong.edit({0,footer}).text = "░";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model,true).empty()) << "One-series owns(any fill) must not bypass its key";
    wrong = good; write(wrong,{0,footer+1},"█ Revenue"); EXPECT_FALSE(merman_rectangular_errors(wrong,model,true).empty());
}
TEST(MermaidChartsNamedSingletonOracle, ExactUnicodeNameSpansKeepTheirContinuationAndOwnerIdentity) {
    auto model = named_singleton_handmade_model("bar",false,"北京 é sales"); const auto good = named_singleton_handmade(model);
    ASSERT_TRUE(merman_rectangular_errors(good,model,true).empty()) << explain(merman_rectangular_errors(good,model,true));
    const int footer = good.height-2; EXPECT_TRUE(good.at({3,footer}).text.empty());
    auto wrong = good; wrong.edit({3,footer}).text = "x"; EXPECT_FALSE(merman_rectangular_errors(wrong,model,true).empty());
    wrong = good; wrong.edit({2,footer}).text = "北"; wrong.edit({4,footer}).text = "北";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model,true).empty());
    auto changed = model; changed.ownership.series[0].name = "北京 é SaleS";
    EXPECT_FALSE(merman_rectangular_errors(good,changed,true).empty());
}
TEST(MermaidChartsNamedSingletonOracle, NumericDefaultCoordinatesNeedAllTicksSamplesAndSourceSelectedArms) {
    // Source-backed values of 88050.../f248...; native auto-domain includes0,
    // while the pinned primary compatibility model's automatic min is23.
    auto model = named_singleton_handmade_model("line",false,"lineTitle");
    model.band = false; model.category_names.clear(); model.x_first = 1; model.x_last = 3;
    model.authored_y.reset(); model.y_first = 0; model.y_last = 56.6;
    model.ownership.category_title = "xAxisName"; model.ownership.value_title = "yAxisName";
    model.ownership.series[0].values = {23,45,56.6}; model.points = {{{1,23},{2,45},{3,56.6}}};
    const auto good = named_singleton_handmade(model);
    ASSERT_TRUE(merman_rectangular_errors(good,model,true).empty()) << explain(merman_rectangular_errors(good,model,true));
    auto wrong = good; const int footer = good.height-2;
    for (int x = 0; x < good.width; ++x) wrong.edit({x,footer}).text = " ";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model,true).empty());
    Work tick_work; const auto middle_tick = find_text(good,"2",tick_work); ASSERT_EQ(middle_tick.size(),1u);
    wrong = good; wrong.edit(middle_tick.front()).text = "9";
    EXPECT_FALSE(merman_rectangular_errors(wrong,model,true).empty()) << "Every actual numeric tick keeps its source coordinate";
    auto changed = model; changed.points[0][1].x = 2.25;
    EXPECT_FALSE(merman_rectangular_errors(good,changed,true).empty());
    changed = model; changed.ownership.series[0].values[1] = 40; changed.points[0][1].value = 40;
    EXPECT_FALSE(merman_rectangular_errors(good,changed,true).empty());
}
TEST(MermaidChartsNamedSingletonOracle, NegativeEndpointAndReferenceZeroHeightStillNeedRealSourcePorts) {
    auto negative = named_singleton_handmade_model("bar",false,"barTitle with space");
    negative.authored_y.reset(); negative.y_first = -4; negative.y_last = 4;
    negative.ownership.series[0].values = {-4,4}; negative.points = {{{0,-4},{1,4}}};
    const auto good = named_singleton_handmade(negative);
    ASSERT_TRUE(merman_rectangular_errors(good,negative,true).empty()) << explain(merman_rectangular_errors(good,negative,true));
    auto wrong = good; wrong.edit({6,19}).text = " ";
    EXPECT_FALSE(merman_rectangular_errors(wrong,negative,true).empty());
    wrong = good; wrong.edit({9,20}).text = "⊕"; EXPECT_FALSE(merman_rectangular_errors(wrong,negative,true).empty());
    auto positive = named_singleton_handmade_model("bar",false,"sample bar");
    positive.authored_y = {{10,14}}; positive.y_first = 10; positive.y_last = 14;
    positive.ownership.series[0].values = {10,14}; positive.points = {{{0,10},{1,14}}};
    const auto zero = named_singleton_handmade(positive);
    ASSERT_TRUE(merman_rectangular_errors(zero,positive,true).empty()) << explain(merman_rectangular_errors(zero,positive,true));
    wrong = zero; wrong.edit({6,20}).text = "┼"; EXPECT_FALSE(merman_rectangular_errors(wrong,positive,true).empty());
}
TEST(MermaidChartsNamedSingletonOracle, CompleteBandTailAndValueDependenciesAreNotLostThroughNames) {
    // Complete source38399... has eight validated raw values and exactly
    // seven retained category slots; the raw99 tail must not replace67.
    auto retained = named_singleton_handmade_model("bar",false,"sample bar");
    retained.ownership.title = "Basic xychart with many categories";
    retained.ownership.category_title = "this is x axis"; retained.ownership.value_title = "yaxisText";
    retained.category_names = {"category1","category 2","category3","category4","category5","category6","category7"};
    retained.authored_y = {{10,150}}; retained.y_first = 10; retained.y_last = 150;
    retained.ownership.series[0].values = {52,96,35,10,87,34,67};
    retained.points = {{{0,52},{1,96},{2,35},{3,10},{4,87},{5,34},{6,67}}};
    const auto complete = named_singleton_handmade(retained);
    ASSERT_TRUE(merman_rectangular_errors(complete,retained,true).empty()) << explain(merman_rectangular_errors(complete,retained,true));
    auto wrong_tail = retained; wrong_tail.ownership.series[0].values.back() = 99; wrong_tail.points[0].back().value = 99;
    EXPECT_FALSE(merman_rectangular_errors(complete,wrong_tail,true).empty());
    for (const bool horizontal : {false,true}) {
        auto model = named_singleton_handmade_model("bar",horizontal);
        model.show_values = true;
        EXPECT_EQ(merman_rectangular_profile(model,true).pending,"inside_outside_or_terminal_value_label_profile_pending");
        for (const bool outside : {false,true}) {
            const std::string source = "---\nconfig:\n  xyChart:\n    showDataLabel: true\n    showDataLabelOutsideBar: "+
                std::string(outside ? "true" : "false")+"\n---\nxychart"+(horizontal ? " horizontal" : "")+
                "\nx-axis [A,B]\ny-axis 0 --> 4\nbar \"Revenue\" [2,4]\n";
            source_fallback(source); // Valid positive dependency, never an invalid/drawing pass.
        }
    }
    source_fallback("xychart\nx-axis [A,B]\ny-axis 0 --> 4\nbar \"Revenue\" [2,4,garbage]\n");
    source_fallback("xychart\nx-axis [A,B]\ny-axis 0 --> 4\nbar \"Revenue\" [2,4,1e999]\n");
}
TEST(MermaidChartsNamedSingletonOracle, NamesAndDataShareExistingWorkCanvasAndLabelBounds) {
    auto model = named_singleton_handmade_model(); const auto good = named_singleton_handmade(model);
    auto wide = good; wide.cells.resize(241); wide.width = 241; wide.height = 1;
    EXPECT_THROW((void)merman_rectangular_errors(wide,model,true),std::runtime_error);
    auto changed = model; changed.ownership.series[0].name = std::string(256,'x');
    EXPECT_NO_THROW({ const auto errors = merman_rectangular_errors(good,changed,true); EXPECT_FALSE(errors.empty()); });
    changed.ownership.series[0].name.push_back('x');
    EXPECT_THROW((void)merman_rectangular_errors(good,changed,true),std::runtime_error);
    auto profile = merman_rectangular_profile(model,true); ASSERT_TRUE(profile.pending.empty());
    Work exhausted{0}; EXPECT_THROW((void)xy_candidate_errors(good,profile.view.chart,Axes{2,0,20,14},exhausted,true),std::runtime_error);
    auto late = good; write(late,{0,good.height-1},"EXTRA"); EXPECT_FALSE(merman_rectangular_errors(late,model,true).empty());
}
} // namespace

namespace {
TEST(MermaidChartsSemantics, CompleteMermanNamedSingleton9AndEligibleSixSourceGeometry) {
    const std::set<std::string> expected_ids{
        "merman/0cfc570b349a718a0f18","merman/2b48cb647797484bdb43","merman/38399d9b6f84b66f397b",
        "merman/40cbcbf662ff791b8d4a","merman/506f4a1e821982f95384","merman/7068173027edf7cd3157",
        "merman/7c40b524079c65f8b63f","merman/88050d11f4a8870db912","merman/f248b6991b99619a837a"};
    const std::set<std::string> expected_eligible{
        "merman/38399d9b6f84b66f397b","merman/40cbcbf662ff791b8d4a","merman/506f4a1e821982f95384",
        "merman/7068173027edf7cd3157","merman/88050d11f4a8870db912","merman/f248b6991b99619a837a"};
    std::set<std::string> visited,eligible_ids;
    json::JsonMutDoc report; auto root = report.object(); auto rows = root.make_arr();
    root.set("kind","complete_source_named_singleton9_with_six_native_owner_geometry_obligations");
    root.set("source_inventory_sha256","a9da8125a4ffbd3838f2caf9e615152c357ee0d08f12a265233b7fd1bb36bdbc");
    root.set("native_contract_sha256","e82a0f46c77c8b00cb96187e408e9740eaef91c147af2b930f8b157c86401ad6");
    root.set("qualification","All9 source/model/provenance bindings retained. Singleton footer identity is a native projection, not the original ASCII smoke predicate. Six strong geometry obligations; three independent indexed/subcell dependencies remain unmet. No complete certification.");
    int eligible = 0,pending = 0,matches = 0,source_count = 0,candidates = 0,active_source = 0,geometry_errors = 0,oracle_errors = 0;
    int observation_errors = 0,eligible_oracle_errors = 0;
    ASSERT_NO_THROW(merman_xy102_native_batch([&](JsonVal row,JsonVal fact,const MermanXY& model) {
        if (model.ownership.series.size() != 1 || model.named_series.size() != 1 || !model.named_series.front()) return;
        const auto id = row.get_string("id"); visited.insert(id); SCOPED_TRACE(id);
        const auto profile = merman_rectangular_profile(model,true); const bool active = profile.pending.empty();
        EXPECT_EQ(active,expected_eligible.contains(id)); if (active) { ++eligible; eligible_ids.insert(id); } else ++pending;
        auto item = root.make_obj(); item.set("id",id); item.set("input_sha256",fact.get_string("input_sha256"));
        item.set("primary_model_sha256",fact.get("primary_model_reference").get_string("sha256"));
        item.set("owner_name",model.ownership.series.front().name); item.set("owner_type",model.ownership.series.front().type);
        item.set("eligible_geometry",active); item.set("dependency_pending",profile.pending);
        bool observed = false,is_source = false,oracle_error = false; std::vector<std::string> errors;
        try {
            const auto source = row.get_string("input"); auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
            auto ordinary = loom::ui::render_markdown(fenced(source,"text")); const auto wide = capture(element,240,256);
            is_source = same_source_surface(wide,capture(ordinary,240,256)); observed = true;
            if (!active) errors.push_back("Independent named-owner dependency remains unmet: "+profile.pending);
            else if (is_source) errors.push_back("Eligible authored singleton owner retained as source; complete geometry/key unmet");
            else errors = merman_rectangular_errors(wide,model,true);
            const auto narrow = capture(element,40,256); const bool narrow_source = same_source_surface(narrow,capture(ordinary,40,256));
            if (active && !is_source && errors.empty() && !narrow_source)
                for (const auto& error : merman_rectangular_errors(narrow,model,true)) errors.push_back("Narrow named-owner geometry: "+error);
            const bool restored = capture(element,240,256) == wide;
            const bool sentinel = wide.text().find(kSentinel) != std::string::npos && narrow.text().find(kSentinel) != std::string::npos;
            if (!restored) errors.push_back("Named-owner width restoration changed the complete surface");
            if (!sentinel) errors.push_back("Named-owner diagram lost following content");
            EXPECT_TRUE(restored); EXPECT_TRUE(sentinel); item.set("restored_identically",restored); item.set("following_content_retained",sentinel);
        } catch (const std::exception& error) { oracle_error = true; errors.push_back("Explicit named-owner oracle error: "+std::string(error.what())); }
        if (!observed) ++observation_errors; else if (is_source) ++source_count; else ++candidates;
        if (oracle_error) { ++oracle_errors; eligible_oracle_errors += active; ADD_FAILURE() << explain(errors); }
        else if (active) { if (is_source) ++active_source; else if (errors.empty()) ++matches; else ++geometry_errors; EXPECT_TRUE(errors.empty()) << explain(errors); }
        item.set("observed",!observed ? "observation_error" : (is_source ? "source_fallback" : "candidate_art"));
        item.set("outcome",oracle_error ? "oracle_error" : (!active ? "dependency_pending" : (is_source ? "source_fallback" : (errors.empty() ? "geometry_match_pending_review" : "geometry_error"))));
        auto failures = root.make_arr(); for (const auto& error : errors) failures.append(root.make_str(error)); item.add("errors",failures); rows.append(item);
    }));
    EXPECT_EQ(visited,expected_ids); EXPECT_EQ(eligible_ids,expected_eligible); EXPECT_EQ(eligible,6); EXPECT_EQ(pending,3);
    EXPECT_EQ(source_count+candidates+observation_errors,9); EXPECT_EQ(matches+active_source+geometry_errors+eligible_oracle_errors,6);
    root.set("complete_source_inputs",static_cast<int>(visited.size())); root.set("eligible_geometry_inputs",eligible);
    root.set("independent_dependency_inputs_pending",pending); root.set("geometry_matches_pending_review",matches);
    root.set("source_fallback_observations",source_count); root.set("candidate_art_observations",candidates); root.set("eligible_source_fallback",active_source);
    root.set("geometry_errors",geometry_errors); root.set("oracle_errors",oracle_errors); root.set("eligible_oracle_errors",eligible_oracle_errors);
    root.set("observation_errors",observation_errors); root.set("original_cohort_function_applications_pending",9);
    root.set("original_global_function_applications_pending",108); root.set("certified_complete_geometry",0); root.add("cases",rows); report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto path = std::filesystem::path(LOOM_MERMAID_REPORT_DIR)/"mermaid-merman-named-singleton9-semantics.json";
    std::ofstream out(path,std::ios::binary); ASSERT_TRUE(out && (out << report.to_pretty_string() << '\n')) << "Cannot write " << path;
#endif
}
} // namespace

namespace {
std::string named_singleton_source(const MermanXY& model,bool named = true,std::string_view prefix = {}) {
    const auto& m = model.ownership; const auto& plot = m.series.front();
    std::string source(prefix); source += m.horizontal ? "xychart horizontal\n" : "xychart\n";
    if (!m.title.empty()) source += "title \""+m.title+"\"\n";
    source += "x-axis "; if (!m.category_title.empty()) source += "\""+m.category_title+"\" ";
    source += "[A,B]\ny-axis "; if (!m.value_title.empty()) source += "\""+m.value_title+"\" ";
    source += "0 --> 4\n"+plot.type+" "; if (named) source += "\""+plot.name+"\" ";
    return source+"["+numeric_x_label(plot.values[0])+","+numeric_x_label(plot.values[1])+"]\n";
}
MermanXY named_singleton_unnamed_partner(MermanXY model) {
    model.named_series.front() = false;
    model.ownership.series.front().name = model.ownership.series.front().type == "bar" ? "Bar 1" : "Line 1";
    return model;
}
std::vector<std::string> named_singleton_real_source_errors(std::string_view source,const MermanXY& model,bool named) {
    std::vector<std::string> errors; auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
    auto ordinary = loom::ui::render_markdown(fenced(source,"text")); const auto wide = capture(element,240,256);
    auto geometry = [&](const Grid& grid) { return model.show_values ? merman_value_errors(grid,model) : merman_rectangular_errors(grid,model,named); };
    if (same_source_surface(wide,capture(ordinary,240,256))) errors.push_back("Legal named-source profile retained as source; complete owner geometry unmet");
    else errors = geometry(wide);
    const auto narrow = capture(element,40,256);
    if (!same_source_surface(narrow,capture(ordinary,40,256)))
        for (const auto& error : geometry(narrow)) errors.push_back("Narrow paired-source geometry: "+error);
    if (!(capture(element,240,256) == wide)) errors.push_back("Paired named source changed exact restored cells");
    if (wide.text().find(kSentinel) == std::string::npos || narrow.text().find(kSentinel) == std::string::npos)
        errors.push_back("Paired named source lost following content");
    return errors;
}
void named_singleton_dependency_source(std::string_view source) {
    SCOPED_TRACE(source);
    auto element = loom::ui::render_markdown(fenced(source,"mermaid")); auto ordinary = loom::ui::render_markdown(fenced(source,"text"));
    const auto wide = capture(element,240,256); EXPECT_TRUE(same_source_surface(wide,capture(ordinary,240,256)));
    const auto narrow = capture(element,40,256); EXPECT_TRUE(same_source_surface(narrow,capture(ordinary,40,256)));
    EXPECT_TRUE(capture(element,240,256) == wide);
    EXPECT_NE(wide.text().find(kSentinel),std::string::npos); EXPECT_NE(narrow.text().find(kSentinel),std::string::npos);
}
TEST(MermaidChartsNamedSingletonResources, LegalSourceKeysAndIndependentDependenciesHaveStrongUnnamedPartners) {
    // Malformed hand-built expected models must fail before any source-profile
    // projection/table index. They are not invalid-Mermaid rendering passes.
    const auto valid = named_singleton_handmade_model(); const auto good = named_singleton_handmade(valid);
    auto reject_model = [&](const MermanXY& invalid) {
        EXPECT_FALSE(merman_rectangular_profile(invalid,true).pending.empty());
        EXPECT_FALSE(merman_rectangular_errors(good,invalid,true).empty());
    };
    auto invalid = valid; invalid.y_first = invalid.y_last = 2; reject_model(invalid);
    invalid = valid; invalid.y_first = -std::numeric_limits<double>::max(); invalid.y_last = std::numeric_limits<double>::max();
    reject_model(invalid);
    for (const double bad : {std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()}) {
        invalid = valid; invalid.y_first = bad; reject_model(invalid);
        invalid = valid; invalid.y_last = bad; reject_model(invalid);
        invalid = valid; invalid.ownership.series.front().values[0] = bad; invalid.points.front()[0].value = bad; reject_model(invalid);
        invalid = valid; invalid.points.front()[0].x = bad; reject_model(invalid);
    }
    invalid = valid; invalid.ownership.series.front().values.pop_back(); reject_model(invalid);
    invalid = valid; invalid.points.front().pop_back(); reject_model(invalid);
    invalid = valid; invalid.points.front()[0].value = 3; reject_model(invalid);
    invalid = valid; invalid.ownership.series.front().values.assign(257,2); invalid.points.front().assign(257,MermanPoint{0,2}); reject_model(invalid);
    // Legal source-side names/false flags require drawings, not merely a key or
    // text presence. These are real first-red admission requirements on e28.
    for (const bool horizontal : {false,true}) for (const std::string& type : std::vector<std::string>{"bar","line"}) {
        auto model = named_singleton_handmade_model(type,horizontal);
        for (const std::string& prefix : std::vector<std::string>{"",
            "---\nconfig:\n  xyChart:\n    showDataLabel: false\n---\n",
            "%%{init: {\"xyChart\": {\"showDataLabel\": false}}}%%\n"}) {
            const auto source = named_singleton_source(model,true,prefix); SCOPED_TRACE(source);
            const auto errors = named_singleton_real_source_errors(source,model,true); EXPECT_TRUE(errors.empty()) << explain(errors);
        }
        model.ownership.series.front().name = "北京 é sales";
        auto source = named_singleton_source(model); SCOPED_TRACE(source);
        auto errors = named_singleton_real_source_errors(source,model,true); EXPECT_TRUE(errors.empty()) << explain(errors);
        model = named_singleton_handmade_model(type,horizontal);
        const auto caption = (type == "bar" ? "█ " : "─ ")+model.ownership.series.front().name;
        model.ownership.title = caption; model.ownership.category_title = caption; model.ownership.value_title = "Value";
        source = named_singleton_source(model); SCOPED_TRACE(source);
        errors = named_singleton_real_source_errors(source,model,true); EXPECT_TRUE(errors.empty()) << explain(errors);
        // Valid delimiter-bearing names are an independent identity dependency.
        model = named_singleton_handmade_model(type,horizontal,"Revenue + Planned");
        EXPECT_EQ(merman_rectangular_profile(model,true).pending,"ambiguous_composite_owner_name_profile_pending");
        named_singleton_dependency_source(named_singleton_source(model));
        auto partner = named_singleton_unnamed_partner(model);
        source = named_singleton_source(partner,false);
        errors = named_singleton_real_source_errors(source,partner,false); EXPECT_TRUE(errors.empty()) << explain(errors);
        // Distinct source values at the same physical endpoint need precision
        // readouts; this new named profile has not declared such extra rows.
        model = named_singleton_handmade_model(type,horizontal);
        model.ownership.series.front().values = {2,2.001}; model.points = {{{0,2},{1,2.001}}};
        EXPECT_EQ(merman_rectangular_profile(model,true).pending,"named_singleton_precision_disclosure_profile_pending");
        named_singleton_dependency_source(named_singleton_source(model));
        partner = named_singleton_unnamed_partner(model); source = named_singleton_source(partner,false);
        errors = named_singleton_real_source_errors(source,partner,false); EXPECT_TRUE(errors.empty()) << explain(errors);
        const auto grid = capture(loom::ui::render_markdown(fenced(source,"mermaid")),240,256);
        Work precise;
        EXPECT_EQ(find_text(grid,partner.ownership.series.front().name+" A=2",precise).size(),1u);
        EXPECT_EQ(find_text(grid,partner.ownership.series.front().name+" B=2.001",precise).size(),1u);
    }
    // Preserve this exact formerly unmet source as complete geometry. It is
    // not an invalid-input fallback once singleton owner keys are implemented.
    {
        auto model = named_singleton_handmade_model("line");
        model.authored_y.reset(); model.y_first = 0; model.y_last = 2;
        model.ownership.series.front().values = {1,2}; model.points = {{{0,1},{1,2}}};
        const std::string source = "xychart\nx-axis [A,B]\nline \"Revenue\" [1,2]\n";
        const auto errors = named_singleton_real_source_errors(source,model,true);
        EXPECT_TRUE(errors.empty()) << explain(errors);
    }
    // Named+enabled-value source semantics remain separate from ordinary
    // singleton naming. Their unnamed counterparts must retain actual values.
    for (const bool horizontal : {false,true}) for (const bool outside : {false,true}) {
        auto model = named_singleton_handmade_model("bar",horizontal); model.show_values = true; model.outside_values = outside;
        const std::string prefix = "---\nconfig:\n  xyChart:\n    showDataLabel: true\n    showDataLabelOutsideBar: "+std::string(outside ? "true" : "false")+"\n---\n";
        named_singleton_dependency_source(named_singleton_source(model,true,prefix));
        auto partner = named_singleton_unnamed_partner(model); const auto source = named_singleton_source(partner,false,prefix); SCOPED_TRACE(source);
        const auto errors = named_singleton_real_source_errors(source,partner,false); EXPECT_TRUE(errors.empty()) << explain(errors);
    }
}
} // namespace

// Source-authored resource controls; no original vendor denominator is added.
// A name of238 display cells plus the two-cell swatch/space fills240 exactly.
// Names256/257 both fall back publicly because the footer exceeds the canvas;
// those two source observations do not independently isolate the parser cap.
namespace {
TEST(MermaidChartsNamedSingletonResources, WholeFooterNamesRespectExistingCellCanvasAndLabelLimits) {
    for (const bool horizontal : {false,true}) for (const std::string& type : std::vector<std::string>{"bar","line"}) {
        for (const bool wide_unicode : {false,true}) {
            auto name_for_cells = [&](int cells) {
                std::string result;
                if (wide_unicode) { while (cells >= 2) { result += "北"; cells -= 2; } }
                result.append(static_cast<std::size_t>(cells),'a'); return result;
            };
            const auto name = name_for_cells(238);
            SCOPED_TRACE(type+(horizontal ? " horizontal " : " vertical ")+(wide_unicode ? "CJK" : "ASCII"));
            EXPECT_EQ(ftxui::string_width(name),238);
            EXPECT_EQ(ftxui::string_width((type=="bar" ? "█ " : "─ ")+name),240);
            const auto model = named_singleton_handmade_model(type,horizontal,name);
            const auto source = named_singleton_source(model);
            auto element = loom::ui::render_markdown(fenced(source,"mermaid"));
            auto ordinary = loom::ui::render_markdown(fenced(source,"text"));
            const auto wide = capture(element,240,256);
            EXPECT_FALSE(same_source_surface(wide,capture(ordinary,240,256)));
            const auto errors = merman_rectangular_errors(wide,model,true);
            EXPECT_TRUE(errors.empty()) << explain(errors);
            Work work; const auto positions = find_text(wide,name,work);
            EXPECT_EQ(positions.size(),1u);
            if (positions.size()==1) {
                const auto at = positions.front(); EXPECT_EQ(at.x,2);
                if (wide_unicode) {
                    EXPECT_EQ(wide.at({238,at.y}).text,"北");
                    EXPECT_TRUE(wide.at({239,at.y}).text.empty());
                } else EXPECT_EQ(wide.at({239,at.y}).text,"a");
            }
            const auto narrow = capture(element,40,256);
            EXPECT_TRUE(same_source_surface(narrow,capture(ordinary,40,256)));
            EXPECT_TRUE(capture(element,240,256)==wide);
            EXPECT_NE(wide.text().find(kSentinel),std::string::npos);
            EXPECT_NE(narrow.text().find(kSentinel),std::string::npos);
            for (const int cells : {239,256,257}) {
                auto over = model; over.ownership.series.front().name = name_for_cells(cells);
                SCOPED_TRACE(cells); EXPECT_EQ(ftxui::string_width(over.ownership.series.front().name),cells);
                named_singleton_dependency_source(named_singleton_source(over));
            }
        }
    }
}
} // namespace

namespace {
// A valid empty state has source meaning but no data geometry. This private
// oracle is deliberately independent of the nonempty scale/series decoder.
struct XY26Empty {
    bool horizontal=false;
    std::optional<std::string> title;
    std::string x_title,y_title;
    std::vector<std::string> categories;
    std::optional<std::pair<double,double>> domain;
    bool default_display=true,accessibility=false;
};
void xy26_require(bool condition,std::string_view error) {
    if(!condition) throw std::runtime_error(std::string(error));
}
void xy26_label(std::string_view text,Work& work) {
    work.spend(text.size());
    // Glyph roundtrip proves physical cell preservation, not scalar validity.
    // This matches the existing chart text-safety boundary without changing it.
    for(std::size_t at=0;at<text.size();) {
        const auto start=at;const auto lead=static_cast<unsigned char>(text[at++]);
        std::uint32_t scalar=lead;int trailing=0;
        if(lead>=0xc2&&lead<=0xdf) {scalar=lead&0x1f;trailing=1;}
        else if(lead>=0xe0&&lead<=0xef) {scalar=lead&0x0f;trailing=2;}
        else if(lead>=0xf0&&lead<=0xf4) {scalar=lead&0x07;trailing=3;}
        else xy26_require(lead<0x80,"Empty label has invalid UTF8 lead");
        xy26_require(at+static_cast<std::size_t>(trailing)<=text.size(),"Empty label has truncated UTF8");
        for(int unit=0;unit<trailing;++unit) {
            const auto byte=static_cast<unsigned char>(text[at++]);
            xy26_require((byte&0xc0)==0x80,"Empty label has invalid UTF8 continuation");
            scalar=(scalar<<6)|(byte&0x3f);
        }
        work.spend(at-start+1);
        xy26_require(!((trailing==1&&scalar<0x80)||(trailing==2&&scalar<0x800)||
            (trailing==3&&scalar<0x10000)||scalar>0x10ffff||(scalar>=0xd800&&scalar<=0xdfff)),
            "Empty label has overlong/non-scalar UTF8");
        xy26_require(!(scalar<0x20||(scalar>=0x7f&&scalar<=0x9f)||
            scalar==0x061c||scalar==0x200e||scalar==0x200f||scalar==0x2028||scalar==0x2029||
            (scalar>=0x202a&&scalar<=0x202e)||(scalar>=0x2066&&scalar<=0x2069)),
            "Empty label has control/bidi data");
    }
    const auto glyphs=ftxui::Utf8ToGlyphs(std::string(text));
    const auto width=ftxui::string_width(std::string(text));
    std::string restored;
    for(const auto& glyph:glyphs) {work.spend(glyph.size()+1);restored+=glyph;}
    xy26_require(restored==text&&width>=0&&static_cast<std::size_t>(width)==glyphs.size(),"Empty label cannot preserve glyph cells");
    xy26_require(glyphs.size()<=256,"Empty label exceeds existing256-cell cap");
}
void xy26_meaning(const XY26Empty& model,Work& work) {
    xy26_require(model.x_title.empty(),"Empty nonempty X title needs independent role");
    xy26_require(model.default_display&&!model.accessibility,"Empty unreviewed display/accessibility profile");
    if(model.title) {xy26_require(!model.title->empty(),"Empty string title differs from absent title");xy26_label(*model.title,work);}
    xy26_label(model.y_title,work);
    xy26_require(model.categories.size()<=256,"Empty category bound");
    for(const auto& category:model.categories) {xy26_require(!category.empty(),"Empty category text");xy26_label(category,work);}
    if(model.domain) xy26_require(std::isfinite(model.domain->first)&&std::isfinite(model.domain->second)&&
        model.domain->first<model.domain->second&&std::isfinite(model.domain->second-model.domain->first),"Empty unsupported directed/nonfinite domain");
}
XY26Empty xy26_decode(JsonVal model,JsonVal facts,Work& work) {
    xy26_require(model.is_obj()&&model.get_string("type")=="xychart","Empty family/schema mismatch");
    xy26_require(facts.get("authored_config_fields").is_arr()&&facts.get("authored_config_fields").size()==0,
                 "Empty authored config profile is not reviewed");
    xy26_require(model.get("plots").is_arr()&&model.get("plots").size()==0,"Empty requires actual no-plots inventory");
    XY26Empty out;
    const auto orientation=model.get("orientation");
    xy26_require(orientation.is_str()&&(orientation.as_str()=="vertical"||orientation.as_str()=="horizontal"),"Empty orientation schema");
    out.horizontal=orientation.as_str()=="horizontal";
    const auto title=model.get("title");
    xy26_require(static_cast<bool>(title)&&(title.is_null()||title.is_str()),"Empty title schema");
    if(title.is_str()) {work.spend(title.as_str().size());out.title=std::string(title.as_str());}
    for(const auto key:{"accTitle","accDescr"}) xy26_require(model.get(key)&&model.get(key).is_null(),"Empty accessibility profile is not reviewed");
    const auto x=model.get("xAxis"),y=model.get("yAxis");
    xy26_require(x.is_obj()&&x.get_string("type")=="band"&&x.get("title").is_str()&&x.get("categories").is_arr(),"Empty X axis schema");
    xy26_require(y.is_obj()&&y.get_string("type")=="linear"&&y.get("title").is_str(),"Empty Y axis schema");
    xy26_require(x.get("title").as_str().empty(),"Empty nonempty X title needs independent role");
    work.spend(y.get("title").as_str().size());out.y_title=y.get_string("title");
    xy26_require(x.get("categories").size()<=256,"Empty category bound");
    x.get("categories").iter([&](JsonVal category) {
        xy26_require(category.is_str(),"Empty category type");work.spend(category.as_str().size()+1);out.categories.emplace_back(category.as_str());
    });
    const auto lo=y.get("min"),hi=y.get("max");
    xy26_require(static_cast<bool>(lo)&&static_cast<bool>(hi),"Empty missing range endpoint");
    if(!(lo.is_null()&&hi.is_null())) {
        xy26_require(lo.is_num()&&hi.is_num(),"Empty mixed-null/wrong-type range");
        out.domain={{lo.as_double(),hi.as_double()}};
    }
    const auto config=model.get("config").get("xyChart");
    xy26_require(config.is_obj(),"Empty display config schema");
    for(const auto key:{"showTitle","showLegend","showDataLabel","showDataLabelOutsideBar"}) {
        const auto flag=config.get(key);xy26_require(flag.is_bool(),"Empty display flag type");
        const bool wanted=std::string_view(key)=="showTitle"||std::string_view(key)=="showLegend";
        out.default_display&=flag.as_bool()==wanted;
    }
    for(const auto axis:{"xAxis","yAxis"}) for(const auto key:{"showLabel","showTitle","showTick","showAxisLine"}) {
        const auto flag=config.get(axis).get(key);xy26_require(flag.is_bool(),"Empty axis flag type");out.default_display&=flag.as_bool();
    }
    xy26_meaning(out,work);
    return out;
}
std::vector<std::string> xy26_lines(const XY26Empty& model,Work& work) {
    xy26_meaning(model,work);
    work.spend(64+model.categories.size()*32);
    std::vector<std::string> lines{"XY chart: no data",model.horizontal?"Orientation: horizontal":"Orientation: vertical"};
    if(model.title) lines.push_back("Title: "+*model.title);
    lines.emplace_back("X axis: band");
    if(model.categories.empty()) lines.emplace_back("Categories: none");
    else for(std::size_t n=0;n<model.categories.size();++n) {
        work.spend();lines.push_back("Category "+std::to_string(n+1)+": "+model.categories[n]);
    }
    lines.emplace_back("Y axis: linear");
    if(!model.y_title.empty()) lines.push_back("Y title: "+model.y_title);
    lines.push_back(model.domain?"Range: "+numeric_x_label(model.domain->first)+" --> "+numeric_x_label(model.domain->second):"Range: unspecified");
    xy26_require(lines.size()<=256,"Empty display row cap");
    for(const auto& line:lines) {work.spend(line.size());xy26_require(ftxui::string_width(line)<=240,"Empty display exceeds existing240-cell canvas");}
    return lines;
}
bool xy26_full_sentinel(const Grid& grid,Work& work) {
    bounded(grid);if(grid.width<static_cast<int>(kSentinel.size())) return false;int found=0;
    for(int y=0;y<grid.height;++y) {
        bool same=true;
        for(int x=0;x<grid.width;++x) {
            work.spend();const std::string wanted=x<static_cast<int>(kSentinel.size())?std::string(1,kSentinel[static_cast<std::size_t>(x)]):" ";
            if(grid.at({x,y}).text!=wanted) {same=false;break;}
        }
        found+=same;
    }
    return found==1;
}
std::vector<std::string> xy26_empty_errors(const Grid& grid,const XY26Empty& expected,std::size_t allowance=8000000) {
    bounded(grid);Work work{allowance};
    const auto lines=xy26_lines(expected,work);
    xy26_require(grid.height>static_cast<int>(lines.size()),"Empty state is truncated");
    const auto background=ftxui::Color(ftxui::Color::Default).Print(true);
    bool sentinel=false;
    for(int y=0;y<grid.height;++y) {
        std::vector<std::string> glyphs;
        if(y<static_cast<int>(lines.size())) {work.spend(lines[static_cast<std::size_t>(y)].size());glyphs=ftxui::Utf8ToGlyphs(lines[static_cast<std::size_t>(y)]);}
        else {
            bool row_sentinel=true;
            for(int x=0;x<grid.width;++x) {
                work.spend();const std::string wanted=x<static_cast<int>(kSentinel.size())?std::string(1,kSentinel[static_cast<std::size_t>(x)]):" ";
                if(grid.at({x,y}).text!=wanted) {row_sentinel=false;break;}
            }
            if(row_sentinel) {
                if(sentinel) return {"Empty state has duplicate following sentinel"};
                sentinel=true;glyphs=ftxui::Utf8ToGlyphs(std::string(kSentinel));
            }
        }
        if(glyphs.size()>static_cast<std::size_t>(grid.width)) return {"Empty role row is clipped"};
        for(int x=0;x<grid.width;++x) {
            work.spend();const auto& cell=grid.at({x,y});
            const std::string_view wanted=static_cast<std::size_t>(x)<glyphs.size()?std::string_view(glyphs[static_cast<std::size_t>(x)]):" ";
            if(cell.text!=wanted) return {"Empty role has missing/foreign exact cells"};
            if(!cell.background.empty()&&cell.background!=background) return {"Empty state has foreign background data"};
        }
    }
    if(!sentinel) return {"Empty state lost complete following sentinel"};
    return {};
}
std::vector<std::string> xy26_full102_empty_errors(const Grid& grid,JsonVal facts) {
    const auto reference=facts.get("primary_model_reference");
    const auto path=std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_terminal/merman"/reference.get_string("path");
    const auto bytes=read(path);xy26_require(digest(bytes)==reference.get_string("sha256"),"Empty complete Model SHA changed");
    const auto document=json::parse(bytes);xy26_require(document.has_value(),"Empty complete Model JSON invalid");
    Work work;const auto model=xy26_decode(document->root(),facts,work);
    return xy26_empty_errors(grid,model,work.left);
}
Grid xy26_header_scene() {
    return scene({"XY chart: no data","Orientation: vertical","X axis: band","Categories: none","Y axis: linear","Range: unspecified","",kSentinel});
}
Grid xy26_axis_scene() {
    return scene({"XY chart: no data","Orientation: horizontal","Title: Lost","X axis: band","Category 1: alpha","Category 2: beta","Y axis: linear","Y title: Score","Range: 0 --> 10","",kSentinel});
}
XY26Empty xy26_axis_model() {
    XY26Empty model;model.horizontal=true;model.title="Lost";model.categories={"alpha","beta"};model.y_title="Score";model.domain={{0,10}};return model;
}
template<class Fn> void xy26_native_batch(Fn&& callback) {
    int selected=0;
    merman_xy102_native_batch([&](JsonVal row,JsonVal fact,const MermanXY&) {
        if(fact.get_string("source_obligation")!="public_empty_state_obligation") return;
        const auto reference=fact.get("primary_model_reference");
        const auto path=std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_terminal/merman"/reference.get_string("path");
        const auto bytes=read(path);xy26_require(digest(bytes)==reference.get_string("sha256"),"Empty complete Model SHA changed");
        const auto model=json::parse(bytes);xy26_require(model.has_value(),"Empty complete Model JSON invalid");
        Work work;const auto expected=xy26_decode(model->root(),fact,work);++selected;callback(row,fact,expected,model->root());
    });
    xy26_require(selected==5,"All five exact empty sources must remain present");
}

TEST(MermaidChartsEmpty5Oracle, HeaderOnlyMeansExplicitStateNotBlankOrInventedData) {
    const XY26Empty model;const auto good=xy26_header_scene();
    EXPECT_TRUE(xy26_empty_errors(good,model).empty());
    for(const auto replacement:{"empty","xychart","XY chart: data"}) {
        auto bad=good;for(int x=0;x<bad.width;++x) bad.edit({x,0}).text=" ";write(bad,{0,0},replacement);
        EXPECT_FALSE(xy26_empty_errors(bad,model).empty());
    }
    auto bad=good;bad.edit({0,6}).text="█";EXPECT_FALSE(xy26_empty_errors(bad,model).empty());
    bad=good;bad.edit({0,6}).text="┼";EXPECT_FALSE(xy26_empty_errors(bad,model).empty());
}
TEST(MermaidChartsEmpty5Oracle, TitlesHaveFinalSetterAndIndependentRoleCells) {
    auto model=XY26Empty{};model.title="oneLinertitle";
    const auto good=scene({"XY chart: no data","Orientation: vertical","Title: oneLinertitle","X axis: band","Categories: none","Y axis: linear","Range: unspecified","",kSentinel});
    EXPECT_TRUE(xy26_empty_errors(good,model).empty());
    auto wrong=model;wrong.title="This is a title";EXPECT_FALSE(xy26_empty_errors(good,wrong).empty());
    wrong.title.reset();EXPECT_FALSE(xy26_empty_errors(good,wrong).empty());
    wrong.title="Orientation: vertical";
    const auto collision=scene({"XY chart: no data","Orientation: vertical","Title: Orientation: vertical","X axis: band","Categories: none","Y axis: linear","Range: unspecified","",kSentinel});
    EXPECT_TRUE(xy26_empty_errors(collision,wrong).empty());
    auto missing=collision;for(int x=0;x<missing.width;++x) missing.edit({x,1}).text=" ";
    EXPECT_FALSE(xy26_empty_errors(missing,wrong).empty());
    missing=collision;for(int x=0;x<missing.width;++x) missing.edit({x,2}).text=" ";
    EXPECT_FALSE(xy26_empty_errors(missing,wrong).empty());
}
TEST(MermaidChartsEmpty5Oracle, ExplicitAxesAndAbsentRangesKeepAllFieldIdentities) {
    const auto model=xy26_axis_model();const auto good=xy26_axis_scene();
    EXPECT_TRUE(xy26_empty_errors(good,model).empty());
    auto wrong=model;std::swap(wrong.categories[0],wrong.categories[1]);EXPECT_FALSE(xy26_empty_errors(good,wrong).empty());
    wrong=model;wrong.categories.pop_back();EXPECT_FALSE(xy26_empty_errors(good,wrong).empty());
    wrong=model;wrong.categories.push_back("beta");EXPECT_FALSE(xy26_empty_errors(good,wrong).empty());
    wrong=model;wrong.y_title="Other";EXPECT_FALSE(xy26_empty_errors(good,wrong).empty());
    wrong=model;wrong.domain={{0,9}};EXPECT_FALSE(xy26_empty_errors(good,wrong).empty());
    wrong=XY26Empty{};wrong.domain={{0,1}};EXPECT_FALSE(xy26_empty_errors(xy26_header_scene(),wrong).empty());
}
TEST(MermaidChartsEmpty5Oracle, OrientationAndXTitleNeverBorrowAnotherRole) {
    const auto good=xy26_axis_scene();auto wrong=xy26_axis_model();wrong.horizontal=false;
    EXPECT_FALSE(xy26_empty_errors(good,wrong).empty());
    const auto vertical=scene({"XY chart: no data","Orientation: vertical","Title: Lost","X axis: band","Category 1: alpha","Category 2: beta","Y axis: linear","Y title: Score","Range: 0 --> 10","",kSentinel});
    EXPECT_TRUE(xy26_empty_errors(vertical,wrong).empty());
    wrong.x_title="Score";EXPECT_THROW(xy26_empty_errors(vertical,wrong),std::runtime_error);
    wrong=XY26Empty{};wrong.title="";EXPECT_THROW(xy26_empty_errors(xy26_header_scene(),wrong),std::runtime_error);
    wrong=XY26Empty{};wrong.default_display=false;EXPECT_THROW(xy26_empty_errors(xy26_header_scene(),wrong),std::runtime_error);
}
TEST(MermaidChartsEmpty5Oracle, UnicodeContinuationAndLateInkRemainPhysical) {
    auto model=xy26_axis_model();model.horizontal=false;model.title="空";
    const auto good=scene({"XY chart: no data","Orientation: vertical","Title: 空","X axis: band","Category 1: alpha","Category 2: beta","Y axis: linear","Y title: Score","Range: 0 --> 10","",kSentinel});
    EXPECT_TRUE(xy26_empty_errors(good,model).empty());
    EXPECT_EQ(good.at({7,2}).text,"空");EXPECT_TRUE(good.at({8,2}).text.empty());
    auto bad=good;bad.edit({8,2}).text=" ";EXPECT_FALSE(xy26_empty_errors(bad,model).empty());
    bad=good;bad.edit({7,2}).text="控";EXPECT_FALSE(xy26_empty_errors(bad,model).empty());
    bad=good;bad.edit({9,2}).text="";EXPECT_FALSE(xy26_empty_errors(bad,model).empty());
    bad=good;bad.edit({2,9}).text="!";EXPECT_FALSE(xy26_empty_errors(bad,model).empty());
    bad=good;bad.edit({0,9}).background="foreign";EXPECT_FALSE(xy26_empty_errors(bad,model).empty());
}
TEST(MermaidChartsEmpty5Oracle, ExistingBudgetsAndWholeSentinelRemainBounded) {
    const auto good=xy26_header_scene();
    EXPECT_THROW(xy26_empty_errors(good,XY26Empty{},1),std::runtime_error);
    auto wrong=XY26Empty{};wrong.title=std::string(257,'a');EXPECT_THROW(xy26_empty_errors(good,wrong),std::runtime_error);
    wrong.title=std::string(239,'a');EXPECT_THROW(xy26_empty_errors(good,wrong),std::runtime_error);
    auto bad=good;bad.edit({0,7}).text="!";EXPECT_FALSE(xy26_empty_errors(bad,XY26Empty{}).empty());
    bad=good;bad.edit({good.width-1,7}).text="!";EXPECT_FALSE(xy26_empty_errors(bad,XY26Empty{}).empty());
}
TEST(MermaidChartsEmpty5Oracle, CompleteSourceSchemaRejectsNullCoercionAndNonemptyPlots) {
    std::string base,facts;
    xy26_native_batch([&](JsonVal,JsonVal f,const XY26Empty& model,JsonVal original) {
        if(!model.title&&!model.domain&&base.empty()) {base=original.to_string();facts=f.to_string();}
    });
    ASSERT_FALSE(base.empty());const auto f=json::parse(facts);ASSERT_TRUE(f);
    const auto original=json::parse(base);ASSERT_TRUE(original);Work positive;
    EXPECT_NO_THROW(xy26_decode(original->root(),f->root(),positive));
    auto rejects=[&](const std::function<void(json::JsonMutVal)>& change) {
        json::JsonMutDoc doc;auto root=doc.copy_val(original->root());change(root);doc.set_root(root);
        const auto value=json::parse(doc.to_string());ASSERT_TRUE(value);Work work;
        EXPECT_THROW(xy26_decode(value->root(),f->root(),work),std::runtime_error);
    };
    rejects([](auto root){root.get("xAxis").set("title","Unreviewed X title");});
    rejects([](auto root){root.set("title","");});
    rejects([](auto root){root.set("title",true);});
    rejects([](auto root){root.set("title",1);});
    rejects([](auto root){static_cast<void>(root.remove("title"));});
    rejects([](auto root){root.get("yAxis").set("min",0);});
    rejects([](auto root){root.get("yAxis").set("max",10);});
    rejects([](auto root){root.get("yAxis").set("min","0");root.get("yAxis").set("max",10);});
    rejects([](auto root){static_cast<void>(root.get("yAxis").remove("min"));});
    rejects([](auto root){root.get("plots").append(root.make_obj());});
    rejects([](auto root){root.get("config").get("xyChart").set("showTitle",false);});
    rejects([](auto root){root.get("config").get("xyChart").get("xAxis").set("showLabel",false);});
}
TEST(MermaidChartsSemantics, AllFiveValidEmptySourcesNeedCompleteStatesNotDataGeometry) {
    json::JsonMutDoc report;auto root=report.object();auto entries=root.make_arr();
    root.set("kind","complete_five_source_empty_state_semantics");root.set("qualification","Valid empty states are separate from nonempty drawing; original API/ASCII predicates remain pending.");
    int visited=0,matched=0,sources=0,semantic_errors=0,oracle_errors=0;
    std::set<std::string> ids;
    xy26_native_batch([&](JsonVal row,JsonVal fact,const XY26Empty& expected,JsonVal) {
        ++visited;const auto id=row.get_string("id"),source=row.get_string("input");SCOPED_TRACE(id);ids.insert(id);
        Grid wide,narrow,restored;bool captured=false,narrow_captured=false,fallback=false,narrow_source=false,oracle_error=false;
        std::vector<std::string> errors;
        try {
            auto element=loom::ui::render_markdown(fenced(source,"mermaid"));auto ordinary=loom::ui::render_markdown(fenced(source,"text"));
            wide=capture(element,240,256);fallback=same_source_surface(wide,capture(ordinary,240,256));captured=true;sources+=fallback;
            auto check=[&](const Grid& image) {try {const auto found=xy26_empty_errors(image,expected);errors.insert(errors.end(),found.begin(),found.end());}
                catch(const std::exception& error) {oracle_error=true;errors.push_back(error.what());}};
            if(fallback) errors.emplace_back("Valid complete empty source remains unmet");else check(wide);
            narrow=capture(element,40,256);const auto plain_narrow=capture(ordinary,40,256);narrow_source=same_source_surface(narrow,plain_narrow);narrow_captured=true;restored=capture(element,240,256);
            if(narrow_source) errors.emplace_back("Reviewed concise empty profile fits40 but remains source");else check(narrow);
            if(!(restored==wide)) errors.emplace_back("Empty state restoration changed pixels");
            Work sentinels;
            if(!xy26_full_sentinel(wide,sentinels)||!xy26_full_sentinel(narrow,sentinels)||!xy26_full_sentinel(restored,sentinels)) errors.emplace_back("Empty state physical sentinel incomplete");
        } catch(const std::exception& error) {oracle_error=true;errors.push_back(error.what());}
        if(oracle_error) ++oracle_errors;else if(!fallback&&errors.empty()) ++matched;else if(!fallback) ++semantic_errors;
        EXPECT_TRUE(errors.empty())<<explain(errors);
        auto item=root.make_obj();item.set("id",id);item.set("source_sha256",digest(source));item.set("complete_model_sha256",fact.get("primary_model_reference").get_string("sha256"));
        item.set("observed",!captured?"observation_error":(fallback?"source_fallback":"candidate_art"));item.set("restored_identically",restored==wide);
        item.set("outcome",oracle_error?"oracle_error":(fallback?"empty_state_source_unmet":(errors.empty()?"empty_state_match_pending_review":"empty_state_semantic_error")));
        item.set("narrow_observed",!narrow_captured?"observation_error":(narrow_source?"source_fallback":"candidate_art"));
        auto frame=root.make_arr();for(int y=0;y<wide.height;++y) frame.append(root.make_str(wide.row(y)));item.add("wide_frame",frame);
        auto list=root.make_arr();for(const auto& error:errors) list.append(root.make_str(error));item.add("errors",list);entries.append(item);
    });
    const std::set<std::string> wanted{"merman/7b7ed6ad2b62a73d1288","merman/aad891514338a8ec1149","merman/bbbbb8060c1a4b938c61","merman/d253903164ded24f8da5","merman/e02006a83b444bc3033d"};
    EXPECT_EQ(ids,wanted);EXPECT_EQ(visited,5);EXPECT_EQ(matched,5);
    root.set("exact_sources",visited);root.set("strong_empty_matches_pending_review",matched);root.set("source_unmet",sources);
    root.set("semantic_errors",semantic_errors);root.set("oracle_errors",oracle_errors);root.set("nonempty_drawing_matches",0);root.set("certified_empty_states",0);
    root.set("original_function_case_applications_pending",6);root.add("cases",entries);report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto path=std::filesystem::path(LOOM_MERMAID_REPORT_DIR)/"mermaid-merman-empty5-semantics.json";
    std::ofstream out(path,std::ios::binary);ASSERT_TRUE(out&&(out<<report.to_pretty_string()<<'\n'));
#endif
}
TEST(MermaidChartsEmpty5Resources, CompleteParsingAndUnsupportedFieldsNeverBecomeAnEmptyShortcut) {
    const std::vector<std::string> deferred{
        "xychart-1", "xychart nonsense", "xychart\nunknown statement\n",
        "xychart\nbar []\n", "xychart\nx-axis []\n", "xychart\ny-axis 0 -->\n",
        "xychart\ntitle \"Unclosed\n", "xychart\ntitle \"\"\n",
        "xychart\nx-axis Caption\n", "xychart\ntitle Ready\ntrailing garbage\n",
        "---\nconfig:\n  xyChart:\n    unknownOption: true\n---\nxychart\n",
        "---\nconfig:\n  xyChart:\n    showTitle: false\n---\nxychart\n"};
    for(const auto& source:deferred) {
        SCOPED_TRACE(source);
        auto element=loom::ui::render_markdown(fenced(source,"mermaid"));auto ordinary=loom::ui::render_markdown(fenced(source,"text"));
        const auto wide=capture(element,240,256),plain=capture(ordinary,240,256);
        const auto narrow=capture(element,40,256),plain_narrow=capture(ordinary,40,256),restored=capture(element,240,256);
        EXPECT_TRUE(same_source_surface(wide,plain));EXPECT_TRUE(same_source_surface(narrow,plain_narrow));EXPECT_TRUE(restored==wide);
        Work work;EXPECT_TRUE(xy26_full_sentinel(wide,work));EXPECT_TRUE(xy26_full_sentinel(narrow,work));EXPECT_TRUE(xy26_full_sentinel(restored,work));
    }
    // Zero data still owns two samples and calibrated axes; it is not a
    // no-plot state. This source already draws in the frozen producer.
    const std::string zero_source="xychart\nbar [0,0]\n";
    auto zero_model=basic_xy();zero_model.categories={"1","2"};
    zero_model.domain={{0,1}};zero_model.series.front().values={0,0};
    auto zero_element=loom::ui::render_markdown(fenced(zero_source,"mermaid"));
    auto zero_plain=loom::ui::render_markdown(fenced(zero_source,"text"));
    const auto zero_wide=capture(zero_element,240,256);
    for(const int width:{240,40,240}) {
        const auto grid=capture(zero_element,width,256);
        EXPECT_TRUE(xy_errors(grid,zero_model).empty())<<explain(xy_errors(grid,zero_model));
        EXPECT_FALSE(same_source_surface(grid,capture(zero_plain,width,256)));
        EXPECT_FALSE(xy26_empty_errors(grid,XY26Empty{}).empty());
        Work work;EXPECT_TRUE(xy26_full_sentinel(grid,work));
        if(width==240) EXPECT_TRUE(grid==zero_wide);
    }
    // Existing complete data geometry is still required when a plot is present.
    const std::string source="xychart\nx-axis [A,B]\ny-axis 0 --> 4\nbar [2,4]\n";
    const auto model=merman_handmade_rectangular();auto element=loom::ui::render_markdown(fenced(source,"mermaid"));
    const auto wide=capture(element,240,256);
    EXPECT_TRUE(merman_rectangular_errors(wide,model).empty());
    EXPECT_FALSE(same_source_surface(wide,capture(loom::ui::render_markdown(fenced(source,"text")),240,256)));
    EXPECT_FALSE(xy26_empty_errors(wide,XY26Empty{}).empty());
}
} // namespace

namespace {
TEST(MermaidChartsEmpty5Oracle, MatchingMalformedUtf8OrBidiModelAndPixelsCannotQualifyAsEmpty) {
    auto literal_title_scene = [](const std::string& title) {
        const std::string row = "Title: "+title;
        // Deliberately independent of xy26_lines and any renderer/parser.
        return scene({"XY chart: no data","Orientation: vertical",row,"X axis: band",
                      "Categories: none","Y axis: linear","Range: unspecified","",kSentinel});
    };
    for (const std::string& title : {std::string("A")+"\xcc\x81"+"B",std::string("空")}) {
        SCOPED_TRACE("Valid attached-combining/CJK positive");
        XY26Empty model;model.title=title;Work work;
        EXPECT_NO_THROW(xy26_label(title,work));
        const auto grid=literal_title_scene(title);
        EXPECT_TRUE(xy26_empty_errors(grid,model).empty());
    }
    const std::array<std::pair<std::string,std::string>,2> bad{{
        {"overlong printable U+002D",std::string("A")+std::string("\xc0\xad",2)+"B"},
        {"bidi U+202E",std::string("A")+std::string("\xe2\x80\xae",3)+"B"}
    }};
    for (const auto& [name,title] : bad) {
        SCOPED_TRACE(name);XY26Empty model;model.title=title;
        // FTXUI's raw glyph roundtrip can preserve these bytes as cells. Their
        // agreement with a forged expected Model must not authorize them.
        const auto grid=literal_title_scene(title);Work work;
        EXPECT_THROW(xy26_label(title,work),std::runtime_error);
        EXPECT_THROW((void)xy26_empty_errors(grid,model),std::runtime_error);
    }
}
} // namespace

namespace {
TEST(MermaidChartsEmpty5Sources, TitleTokenJoiningTrimmingAndCanvasBoundariesRemainSourceDerived) {
    const std::vector<std::pair<std::string,std::string>> titles{
        {"title One Two\n","OneTwo"}, {"title One\tTwo\n","OneTwo"},
        {"title \" A \"\n","A"}, {"title \"A B\"\n","A B"},
        {"title \" 空 \"\n","空"}, {"title 空\n","空"},
        {"title \"Before\"\ntitle One Two\n","OneTwo"},
        {"title Before\ntitle \"A B\"\n","A B"},
        {"title \"XY chart: no data\"\n","XY chart: no data"}};
    for(const auto& [statement,title]:titles) {
        SCOPED_TRACE(statement);const auto source="xychart\n"+statement;
        XY26Empty model;model.title=title;
        auto element=loom::ui::render_markdown(fenced(source,"mermaid"));
        auto ordinary=loom::ui::render_markdown(fenced(source,"text"));
        const auto wide=capture(element,240,256);
        for(const int width:{240,40,240}) {
            const auto grid=capture(element,width,256);
            EXPECT_FALSE(same_source_surface(grid,capture(ordinary,width,256)));
            EXPECT_TRUE(xy26_empty_errors(grid,model).empty())<<explain(xy26_empty_errors(grid,model));
            if(width==240) EXPECT_TRUE(grid==wide);
        }
    }
    for(const auto statement:{"title One line\n","title One line\ntitle Valid\n",
                              "title xychart-beta\n","title \" \"\n"})
        named_singleton_dependency_source("xychart\n"+std::string(statement));
    for(const auto config:{"---\nconfig:\n  xyChart:\n    width: 200\n---\n",
                          "%%{init: {\"xyChart\": {\"showDataLabel\": false}}}%%\n"})
        named_singleton_dependency_source(std::string(config)+"xychart\n");
    for(const auto statement:{"y-axis line\n","x-axis [line,A]\n","y-axis 123\n",
                              "y-axis Score1\n","y-axis line 0 --> 10\n"})
        named_singleton_dependency_source("xychart\n"+std::string(statement));
    const std::vector<std::pair<std::string,XY26Empty>> axis_sources{
        {"xychart\nx-axis [\"line\",A]\n",[]{XY26Empty model;model.categories={"line","A"};return model;}()},
        {"xychart\ny-axis \"123\"\n",[]{XY26Empty model;model.y_title="123";return model;}()},
        {"xychart\ny-axis Score Card\n",[]{XY26Empty model;model.y_title="ScoreCard";return model;}()},
        {"xychart\ny-axis Score 0 --> 10\n",[]{XY26Empty model;model.y_title="Score";model.domain={{0,10}};return model;}()}};
    for(const auto& [source,model]:axis_sources) {
        SCOPED_TRACE(source);auto element=loom::ui::render_markdown(fenced(source,"mermaid"));
        const auto wide=capture(element,240,256);
        for(const int width:{240,40,240}) {
            const auto grid=capture(element,width,256);
            EXPECT_TRUE(xy26_empty_errors(grid,model).empty())<<explain(xy26_empty_errors(grid,model));
            if(width==240) EXPECT_TRUE(grid==wide);
        }
    }
    // 233 title cells plus the independent seven-cell role prefix fit240;
    // the next source cell must retain the complete source, not clip the role.
    const std::string title(233,'a');const auto source="xychart\ntitle \""+title+"\"\n";
    XY26Empty model;model.title=title;
    auto element=loom::ui::render_markdown(fenced(source,"mermaid"));
    const auto wide=capture(element,240,256);
    EXPECT_TRUE(xy26_empty_errors(wide,model).empty())<<explain(xy26_empty_errors(wide,model));
    EXPECT_TRUE(same_source_surface(capture(element,40,256),capture(loom::ui::render_markdown(fenced(source,"text")),40,256)));
    EXPECT_TRUE(capture(element,240,256)==wide);
    for(const int cells:{234,256,257})
        named_singleton_dependency_source("xychart\ntitle \""+std::string(static_cast<std::size_t>(cells),'a')+"\"\n");
}
} // namespace

namespace {
MermanXY short_title_dense_hand_model() {
    MermanXY m;
    m.band = false; m.x_first = 0; m.x_last = 2; m.y_first = 0; m.y_last = 4;
    m.authored_x = {{0,2}}; m.authored_y = {{0,4}}; m.named_series = {true};
    m.ownership = XY{"Long numeric","","",false,{{0,4}},{},{{"line","CPU",{1,2,3,2}}}};
    const double step = 2.0/3.0;
    m.points = {{{0,1},{step,2},{2*step,3},{2,2}}};
    return m;
}
Grid short_title_dense_hand_grid() {
    // Independent fixed coordinates; no producer or source-projection function
    // generates this witness. Four source samples occupy x12/33/54/75.
    Grid g{88,26,std::vector<Cell>(88u*26u)};
    write(g,{0,0},"Long numeric");
    for (int y = 1; y <= 21; ++y) g.edit({2,y}).text = "│";
    for (int x = 2; x <= 85; ++x) g.edit({x,21}).text = "─";
    for (int n = 0; n <= 4; ++n) {
        const int y = 21-5*n; write(g,{0,y},std::to_string(n)); g.edit({2,y}).text = n ? "┤" : "┼";
    }
    for (const auto& [x,label] : {std::pair{12,"0"},std::pair{33,"0.6666666666666666"},
            std::pair{54,"1.3333333333333333"},std::pair{75,"2"}}) {
        g.edit({x,21}).text = "┬";
        write(g,{x-(ftxui::string_width(label)-1)/2,22},label);
    }
    const std::array<Point,8> path{{{12,16},{22,16},{22,11},{43,11},{43,6},{64,6},{64,11},{75,11}}};
    auto glyph = [](unsigned bits) -> std::string_view {
        switch (bits) {
            case L: case R: case L|R: return "─";
            case U: case D: case U|D: return "│";
            case R|D: return "╭"; case L|D: return "╮";
            case R|U: return "╰"; case L|U: return "╯";
            default: return "┼";
        }
    };
    std::array<unsigned,88u*26u> path_bits{};
    for (std::size_t n = 1; n < path.size(); ++n) {
        const auto a = path[n-1],b = path[n]; const int dx = (b.x>a.x)-(b.x<a.x),dy = (b.y>a.y)-(b.y<a.y);
        const unsigned forward = dx>0 ? R : dx<0 ? L : dy>0 ? D : U;
        const unsigned backward = dx>0 ? L : dx<0 ? R : dy>0 ? U : D;
        auto p = a;
        while (p != b) {
            auto q = Point{p.x+dx,p.y+dy};
            path_bits[static_cast<std::size_t>(p.y*88+p.x)] |= forward;
            path_bits[static_cast<std::size_t>(q.y*88+q.x)] |= backward; p = q;
        }
    }
    for (std::size_t n = 0; n < path_bits.size(); ++n) if (path_bits[n])
        g.cells[n].text = glyph(path_bits[n]);
    write(g,{0,24},"─ CPU");
    return g;
}
std::vector<std::string> short_title_dense_errors(const Grid& grid,const MermanXY& model,bool named = true) {
    return merman_rectangular_errors(grid,model,named,NumericTickProfile::DenseSource);
}
TEST(MermaidChartsDenseNumericOracle, LongCompleteTokensHaveOwnedPortsAndOneAffineCoordinateMap) {
    const auto model = short_title_dense_hand_model(); const auto good = short_title_dense_hand_grid();
    EXPECT_EQ(merman_rectangular_profile(model,true).pending,"sparse_numeric_tick_profile_pending");
    ASSERT_TRUE(short_title_dense_errors(good,model).empty()) << explain(short_title_dense_errors(good,model));
    auto changed = good; changed.edit({42,22}).text = "5"; // one ULP at the last fractional digit
    EXPECT_FALSE(short_title_dense_errors(changed,model).empty());
    changed = good; changed.edit({25,22}).text = " "; // missing leading zero, token fragments cannot bind
    EXPECT_FALSE(short_title_dense_errors(changed,model).empty());
    changed = good; changed.edit({33,21}).text = "─";
    EXPECT_FALSE(short_title_dense_errors(changed,model).empty());
    changed = good; changed.edit({33,11}).text = " "; // actual source sample/shaft is not a table
    EXPECT_FALSE(short_title_dense_errors(changed,model).empty());
    changed = good; changed.edit({16,16}).text = "┬"; // one-cell unused-arm spur
    EXPECT_FALSE(short_title_dense_errors(changed,model).empty());
    changed = good; changed.edit({16,16}).text = "━";
    EXPECT_FALSE(short_title_dense_errors(changed,model).empty());
    changed = good; write(changed,{2,24},"GPU");
    EXPECT_FALSE(short_title_dense_errors(changed,model).empty());
    changed = good; changed.edit({0,0}).text = "X";
    EXPECT_FALSE(short_title_dense_errors(changed,model).empty());
    changed = good; write(changed,{0,25},"0.6666666666666666");
    EXPECT_FALSE(short_title_dense_errors(changed,model).empty());
    auto wrong = model; wrong.points[0][1].x = std::nextafter(wrong.points[0][1].x,1.0);
    EXPECT_FALSE(short_title_dense_errors(good,wrong).empty());
    wrong = model; wrong.ownership.series[0].values[1] = 2.25; wrong.points[0][1].value = 2.25;
    EXPECT_FALSE(short_title_dense_errors(good,wrong).empty());
    wrong = model; wrong.x_last = 3; wrong.authored_x = {{0,3}};
    EXPECT_FALSE(short_title_dense_errors(good,wrong).empty());
}
TEST(MermaidChartsDenseNumericOracle, SourcePreflightUsesSharedWorkAndKeepsEveryOldDefaultProfile) {
    const auto model = short_title_dense_hand_model(); Work full;
    EXPECT_TRUE(dense_numeric_model_errors(model,full).empty()); EXPECT_LT(full.left,8000000u);
    Work exhausted{0}; EXPECT_THROW((void)dense_numeric_model_errors(model,exhausted),std::runtime_error);
    auto bad = model; bad.points[0][1].x = std::numeric_limits<double>::infinity(); Work finite;
    EXPECT_FALSE(dense_numeric_model_errors(bad,finite).empty());
    bad = model; bad.points[0].pop_back(); Work ragged;
    EXPECT_FALSE(dense_numeric_model_errors(bad,ragged).empty());
    bad = model; bad.ownership.horizontal = true; Work horizontal;
    EXPECT_FALSE(dense_numeric_model_errors(bad,horizontal).empty());
    bad = model; bad.band = true; Work categorical;
    EXPECT_FALSE(dense_numeric_model_errors(bad,categorical).empty());
    bad = model; bad.ownership.series[0].values.resize(65); bad.points[0].resize(65); Work ticks;
    EXPECT_FALSE(dense_numeric_model_errors(bad,ticks).empty());
    EXPECT_EQ(merman_rectangular_profile(model,true).pending,"sparse_numeric_tick_profile_pending");
    EXPECT_TRUE(merman_rectangular_errors(merman_handmade_fixed_bars(),merman_handmade_rectangular()).empty());
}
MermanXY short_title_complete_cpu_model() {
    MermanXY m; m.band = false; m.x_first = 0; m.x_last = 10; m.y_first = 0; m.y_last = 100;
    m.authored_x = {{0,10}}; m.authored_y = {{0,100}}; m.named_series = {true};
    m.ownership.title = "Too short for a title";
    m.ownership.category_title = "Elapsed Time"; m.ownership.value_title = "Percentage (%)";
    m.ownership.domain = {{0,100}}; m.ownership.series = {{"line","CPU",{10,20,30,40,50,60,70,80,90,100}}};
    m.points.resize(1); const double step = 10.0/9.0;
    for (std::size_t n = 0; n < 10; ++n) m.points[0].push_back({n*step,static_cast<double>(10*(n+1))});
    return m;
}
void short_title_expect_full_public_source(std::string_view source,const MermanXY& model,bool named) {
    auto node = loom::ui::render_markdown(fenced(source,"mermaid"));
    auto ordinary = loom::ui::render_markdown(fenced(source,"text"));
    const auto wide = capture(node,240,256),narrow = capture(node,40,256),restored = capture(node,240,256);
    EXPECT_EQ(wide,restored);
    for (const auto* frame : {&wide,&narrow,&restored}) {
        Work sentinel; EXPECT_EQ(find_text(*frame,kSentinel,sentinel).size(),1u);
    }
    ASSERT_FALSE(same_source_surface(wide,capture(ordinary,240,256))) << "Source-positive drawing obligation remains unmet";
    const auto errors = short_title_dense_errors(wide,model,named);
    ASSERT_TRUE(errors.empty()) << explain(errors);
    EXPECT_TRUE(short_title_dense_errors(restored,model,named).empty());
    if (!same_source_surface(narrow,capture(ordinary,40,256)))
        EXPECT_TRUE(short_title_dense_errors(narrow,model,named).empty());
    auto wrong = model; wrong.ownership.series[0].values[0] += (model.y_last-model.y_first)/5; wrong.points[0][0].value = wrong.ownership.series[0].values[0];
    EXPECT_FALSE(short_title_dense_errors(wide,wrong,named).empty());
    wrong = model; wrong.ownership.series[0].name = "Wrong owner";
    EXPECT_FALSE(short_title_dense_errors(wide,wrong,named).empty());
    wrong = model; wrong.ownership.category_title = "Wrong x-axis title";
    EXPECT_FALSE(short_title_dense_errors(wide,wrong,named).empty());
    wrong = model; wrong.ownership.value_title = "Wrong y-axis title";
    EXPECT_FALSE(short_title_dense_errors(wide,wrong,named).empty());
    wrong = model; wrong.ownership.series[0].values.pop_back(); wrong.points[0].pop_back();
    EXPECT_FALSE(short_title_dense_errors(wide,wrong,named).empty());
    if (!model.ownership.title.empty()) {
        wrong = model; wrong.ownership.title = "Wrong chart title";
        EXPECT_FALSE(short_title_dense_errors(wide,wrong,named).empty());
    }
}
TEST(MermaidChartsDenseNumericSources, FullRawCpuChartKeepsItsNativeTitleAndAllTenSourceSamples) {
    const auto path = std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_official/cases/xychart/nightly-check/should-drop-the-title-when-the-chart-is-too-short.mmd";
    const auto source = read(path);
    ASSERT_EQ(digest(source),"b2735f4436f20e52fddbae28ae5cf087f6a8023ae7b1f137a27d6b114e2e28c9");
    const auto model = short_title_complete_cpu_model(); ASSERT_EQ(model.points[0].size(),10u);
    short_title_expect_full_public_source(source,model,true);
    // This native adaptation preserves the authored title. Original SVG-height
    // omission and browser screenshot predicates remain explicitly unexecuted.
}
TEST(MermaidChartsDenseNumericSources, CompleteFrozenMixedNumericSourceRetainsEveryFractionalSample) {
    int selected = 0;
    ASSERT_NO_THROW(merman_xy102_native_batch([&](JsonVal row,JsonVal,const MermanXY& model) {
        if (row.get_string("id") != "merman/d191355da1e4302cfae5") return;
        ++selected; ASSERT_EQ(row.get_string("sha256"),"d191355da1e4302cfae551e54b8c24ba111076823deb24df68555813bb31d6d7");
        const auto source = row.get_string("input");
        ASSERT_EQ(source,"xychart\n  x-axis 0 --> 2\n  line [0, 1, 0, 1]\n  bar [1, 0, 1, 0]\n");
        ASSERT_EQ(model.ownership.series.size(),2u); ASSERT_EQ(model.points[0].size(),4u);
        short_title_expect_full_public_source(source,model,false);
    }));
    EXPECT_EQ(selected,1);
}
} // namespace

namespace {
TEST(MermaidChartsDenseNumericOracle, CompleteSourceTextAdmissionPrecedesCopiesAndPointGeometry) {
    const auto good=short_title_dense_hand_model();
    auto error_for=[&](const MermanXY& model) {Work work;return explain(dense_numeric_model_errors(model,work));};
    for(const auto field:{"title","category_title","value_title","owner_name"}) {
        auto bad=good;
        if(std::string_view(field)=="title") bad.ownership.title=std::string(257,'T');
        else if(std::string_view(field)=="category_title") bad.ownership.category_title=std::string(257,'X');
        else if(std::string_view(field)=="value_title") bad.ownership.value_title=std::string(257,'Y');
        else bad.ownership.series.front().name=std::string(257,'N');
        SCOPED_TRACE(field);
        // Every original point/domain/value remains valid. Only the source
        // field is poisoned, so failure cannot borrow an invalid x shortcut.
        const auto error=error_for(bad);
        EXPECT_NE(error.find("Dense source"),std::string::npos);
        EXPECT_NE(error.find("safe256-cell admission"),std::string::npos);
    }
    auto bad=good;bad.ownership.series.front().type="area";
    EXPECT_NE(error_for(bad).find("owner type must be bar or line before copy"),std::string::npos);
    bad=good;bad.category_names={"Unused category"};
    EXPECT_NE(error_for(bad).find("cannot ignore residual categorical fields"),std::string::npos);
    bad=good;bad.ownership.categories={"Unused view category"};
    EXPECT_NE(error_for(bad).find("cannot ignore residual categorical fields"),std::string::npos);
    bad=good;bad.ownership.series.front().name.clear();
    EXPECT_NE(error_for(bad).find("owner name exceeds safe256-cell admission"),std::string::npos);
    for(const std::string& text:std::vector<std::string>{std::string("\xc0\xad"),std::string("\xed\xa0\x80"),
            std::string("\xf4\x90\x80\x80"),std::string("\xe2\x80\xae"),std::string("\xc2\x85"),
            std::string("\xcc\x81")+"Lead",std::string("A\tB")}) {
        bad=good;bad.ownership.title=text;
        EXPECT_NE(error_for(bad).find("title exceeds safe256-cell admission"),std::string::npos);
    }
    // No new240-cell field policy: the existing label admission still allows
    // 256 cells. Complete rendered rows retain the separate240 canvas check.
    for(const int length:{241,256}) {
        auto label=good;label.ownership.title=std::string(static_cast<std::size_t>(length),'T');Work work;
        EXPECT_TRUE(dense_numeric_model_errors(label,work).empty());
    }
    auto cjk=good;cjk.ownership.title.clear();for(int n=0;n<128;++n) cjk.ownership.title+="北";
    Work cjk_work;EXPECT_TRUE(dense_numeric_model_errors(cjk,cjk_work).empty());
    cjk.ownership.title+="北";EXPECT_NE(error_for(cjk).find("title exceeds safe256-cell admission"),std::string::npos);
    auto attached=good;attached.ownership.title="A\xcc\x81";Work attached_work;
    EXPECT_TRUE(dense_numeric_model_errors(attached,attached_work).empty());
    Work tight{64};
    try {
        static_cast<void>(dense_numeric_model_errors(good,tight));
        ADD_FAILURE()<<"Tight valid-source Work bypassed copy precharge";
    } catch(const std::runtime_error& error) {
        EXPECT_NE(std::string_view(error.what()).find("Dense source copy precharge exhausted before text/point processing"),std::string_view::npos);
    }
    // Only the optional authored domain is poisoned. Resolved y, every point,
    // owner value and the independent valid grid remain untouched.
    const auto good_grid=short_title_dense_hand_grid();
    for(const auto& domain:std::vector<std::pair<double,double>>{
            {0,8},{1,4},{0,3},{4,0},{0,0},
            {std::numeric_limits<double>::quiet_NaN(),4},
            {0,std::numeric_limits<double>::infinity()},
            {-std::numeric_limits<double>::max(),std::numeric_limits<double>::max()}}) {
        auto wrong=good;wrong.authored_y=domain;
        EXPECT_NE(error_for(wrong).find("authored y domain differs from resolved source range"),std::string::npos);
        EXPECT_FALSE(short_title_dense_errors(good_grid,wrong).empty());
    }
    // An inferred domain remains valid: d191 has no explicit authored_y.
    auto inferred=good;inferred.authored_y.reset();Work inferred_work;
    EXPECT_TRUE(dense_numeric_model_errors(inferred,inferred_work).empty());
    EXPECT_TRUE(short_title_dense_errors(good_grid,inferred).empty());
    // The exact same shared Work is retained; after first-source admission a
    // second call cannot reset the resource ledger.
    Work cumulative{500};std::vector<std::string> first;
    ASSERT_NO_THROW(first=dense_numeric_model_errors(good,cumulative));EXPECT_TRUE(first.empty());
    EXPECT_THROW(static_cast<void>(dense_numeric_model_errors(good,cumulative)),std::runtime_error);
}
} // namespace

namespace {
TEST(MermaidChartsDenseNumericSources, ExactlyOneReviewedDenseC2JoinKeepsTheOtherTwentyDependenciesPending) {
    int visited=0,dense=0,pending=0;std::set<std::string> remaining;
    ASSERT_NO_THROW(merman_xy102_native_batch([&](JsonVal row,JsonVal fact,const MermanXY& model) {
        ++visited;const auto id=row.get_string("id");const bool selected=id==kD191DenseId;
        const auto mode=selected?NumericTickProfile::DenseSource:NumericTickProfile::Short;
        if(selected) {
            ++dense;d191_dense_binding(row,fact,model,mode);
            EXPECT_EQ(merman_rectangular_profile(model).pending,"sparse_numeric_tick_profile_pending");
            EXPECT_TRUE(merman_rectangular_profile(model,false,mode).pending.empty());
        }
        const auto profile=merman_rectangular_profile(model,false,mode);
        const bool value=merman_value_active(model),empty=model.ownership.series.empty()&&fact.get_string("source_obligation")=="public_empty_state_obligation";
        const bool named=model.ownership.series.size()==1&&model.named_series.size()==1&&model.named_series.front()&&merman_rectangular_profile(model,true).pending.empty();
        if(!(selected||value||empty||named||profile.pending.empty())) {++pending;remaining.insert(id);}
    }));
    EXPECT_EQ(visited,102);EXPECT_EQ(dense,1);EXPECT_EQ(pending,20);EXPECT_FALSE(remaining.contains(std::string(kD191DenseId)));
}
TEST(MermaidChartsDenseNumericSources, DenseC2BindingRejectsChangedSourceFieldTypesDomainsAndOwnership) {
    int selected=0;
    ASSERT_NO_THROW(merman_xy102_native_batch([&](JsonVal row,JsonVal fact,const MermanXY& model) {
        if(row.get_string("id")!=kD191DenseId) return;
        ++selected;
        EXPECT_NO_THROW(d191_dense_binding(row,fact,model,NumericTickProfile::DenseSource));
        EXPECT_THROW(d191_dense_binding(row,fact,model,NumericTickProfile::Short),std::runtime_error);
        EXPECT_THROW(d191_dense_binding(row,fact,model,static_cast<NumericTickProfile>(42)),std::runtime_error);
        auto wrong=model;wrong.band=true;EXPECT_THROW(d191_dense_binding(row,fact,wrong,NumericTickProfile::DenseSource),std::runtime_error);
        wrong=model;wrong.authored_x={{0,3}};EXPECT_THROW(d191_dense_binding(row,fact,wrong,NumericTickProfile::DenseSource),std::runtime_error);
        wrong=model;wrong.y_last=2;EXPECT_THROW(d191_dense_binding(row,fact,wrong,NumericTickProfile::DenseSource),std::runtime_error);
        wrong=model;wrong.ownership.series[1].type="line";EXPECT_THROW(d191_dense_binding(row,fact,wrong,NumericTickProfile::DenseSource),std::runtime_error);
        wrong=model;wrong.ownership.series[0].name="Bar 1";EXPECT_THROW(d191_dense_binding(row,fact,wrong,NumericTickProfile::DenseSource),std::runtime_error);
        wrong=model;wrong.ownership.series[0].values[1]=0;wrong.points[0][1].value=0;
        EXPECT_THROW(d191_dense_binding(row,fact,wrong,NumericTickProfile::DenseSource),std::runtime_error);
        wrong=model;wrong.points[0][1].x=std::nextafter(wrong.points[0][1].x,1.0);
        EXPECT_THROW(d191_dense_binding(row,fact,wrong,NumericTickProfile::DenseSource),std::runtime_error);
        wrong=model;wrong.named_series[0]=true;EXPECT_THROW(d191_dense_binding(row,fact,wrong,NumericTickProfile::DenseSource),std::runtime_error);
        auto bad_binding=[&](JsonVal input,const std::function<void(json::JsonMutVal)>& change,bool is_row) {
            json::JsonMutDoc document;auto root=document.copy_val(input);change(root);document.set_root(root);
            const auto mutated=json::parse(document.to_string());ASSERT_TRUE(mutated);
            if(is_row) EXPECT_THROW(d191_dense_binding(mutated->root(),fact,model,NumericTickProfile::DenseSource),std::runtime_error);
            else EXPECT_THROW(d191_dense_binding(row,mutated->root(),model,NumericTickProfile::DenseSource),std::runtime_error);
        };
        bad_binding(row,[](auto root){root.set("id","merman/wrong");},true);
        bad_binding(row,[](auto root){root.set("input","xychart\nline [0,1]\n");},true);
        bad_binding(fact,[](auto root){root.get("primary_model_reference").set("bytes","24093");},false);
        bad_binding(fact,[](auto root){root.get("primary_model_reference").set("sha256","wrong");},false);
        const auto raw=read(std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_terminal/merman"/kD191ProjectionPath);
        const auto parsed=json::parse(raw);ASSERT_TRUE(parsed);EXPECT_NO_THROW(d191_strict_projection(parsed->root()));
        auto rejects=[&](const std::function<void(json::JsonMutVal)>& change) {
            json::JsonMutDoc document;auto root=document.copy_val(parsed->root());change(root);document.set_root(root);
            const auto mutated=json::parse(document.to_string());ASSERT_TRUE(mutated);
            EXPECT_THROW(d191_strict_projection(mutated->root()),std::runtime_error);
        };
        rejects([](auto root){root.get("xAxis").set("min","0");});
        rejects([](auto root){root.get("yAxis").set("max",true);});
        rejects([](auto root){root.get("xAxis").set("title",false);});
        rejects([](auto root){root.get("plots").at(1).set("title","Line 1");});
        rejects([](auto root){root.get("plots").at(0).set("type","bar");});
    }));
    EXPECT_EQ(selected,1);
}

// Whole-source Pie7 native proof. Source text and Model are frozen independently
// of the producer; only actual PixelAt-derived cells authorize geometry.
constexpr std::size_t kPie7SceneLimit = 128000000u;
void pie7_need(bool condition,std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}
bool pie7_resource(std::string_view message) {
    return message.find("exhausted") != std::string_view::npos || message.find("budget") != std::string_view::npos;
}
std::string pie7_read(const std::filesystem::path& path,Work& work) {
    const auto bytes = std::filesystem::file_size(path);
    pie7_need(bytes <= 65536u,"Pie7 bounded immutable asset");
    work.spend(static_cast<std::size_t>(bytes)*8u+4096u);
    std::ifstream input(path,std::ios::binary);
    pie7_need(input.good(),"Pie7 asset open");
    std::string text{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    pie7_need(text.size() == bytes,"Pie7 complete asset read");
    return text;
}
void pie7_publish(const Grid& grid,std::string_view name,json::JsonMutVal root,json::JsonMutVal row,Work& work) {
    bounded(grid);
    work.spend(grid.cells.size()*(4u*sizeof(Cell)+256u)+4096u);
    auto frame = root.make_obj(),pixels = root.make_arr(),rows = root.make_arr();
    frame.set("width",grid.width); frame.set("height",grid.height);
    for (const auto& cell : grid.cells) {
        auto pixel = root.make_obj(); pixel.set("text",cell.text); pixel.set("foreground",cell.foreground); pixel.set("background",cell.background); pixels.append(pixel);
    }
    for (int y=0; y<grid.height; ++y) rows.append(root.make_str(grid.row(y)));
    frame.add("cells",pixels); frame.add("rows",rows); row.add(name,frame);
}
struct Pie7Frames { std::array<Grid,5> views; };
Pie7Frames pie7_observe(std::string_view source,json::JsonMutVal root,json::JsonMutVal row,Work& work) {
    // Pay fenced strings/Markdown ownership before constructing either node.
    work.spend((source.size()+128u)*(32u*sizeof(std::string)+256u)+16384u);
    auto drawing = loom::ui::render_markdown(fenced(source,"mermaid"));
    auto ordinary = loom::ui::render_markdown(fenced(source,"text"));
    drawing->ComputeRequirement(); ordinary->ComputeRequirement();
    const int height = std::max(32,std::max(drawing->requirement().min_y,ordinary->requirement().min_y)+12);
    pie7_need(height<=kMaxHeight,"Pie7 complete public capture height");
    Pie7Frames out;
    constexpr std::array<int,5> widths{{240,40,240,240,40}};
    constexpr std::array<std::string_view,5> names{{"wide_240","narrow_40","restored_240","source_control_240","source_control_40"}};
    for (std::size_t n=0; n<out.views.size(); ++n) {
        out.views[n]=capture_paid(n<3u?drawing:ordinary,widths[n],height,work);
        pie7_publish(out.views[n],names[n],root,row,work);
    }
    return out;
}
std::vector<std::string> pie7_geometry(const Grid& grid,const Pie& model,Work& work) {
    // Own the new caller's temporary candidates, decoded labels and claims.
    // Legacy two-argument calls retain their existing default ledger.
    work.spend(grid.cells.size()*(2u*sizeof(Cell)+64u)+model.slices.size()*1024u+4096u);
    return pie_errors(grid,model,work);
}
void pie7_lifecycle(const Pie7Frames& frames,Work& work) {
    for (const auto& grid : frames.views) pie7_need(find_text(grid,kSentinel,work).size()==1u,"Pie7 unique following token in each view");
    pie7_need(frames.views[0]==frames.views[2],"Pie7 same-node resize restoration");
    if (same_source_surface(frames.views[0],frames.views[3]))
        pie7_need(same_source_surface(frames.views[1],frames.views[4]),"Pie7 whole ordinary lifecycle");
}
Point pie7_unique(const Grid& grid,std::string_view text,Work& work) {
    const auto points=find_text(grid,text,work); pie7_need(points.size()==1u,"Pie7 unique physical mutant target"); return points.front();
}
int pie7_blank_row(const Grid& grid,Work& work) {
    for (int y=0;y<grid.height;++y) {
        bool blank=true;
        for (int x=0;x<grid.width;++x) {work.spend(); if(grid.at({x,y}).text!=" ")blank=false;}
        if(blank)return y;
    }
    throw std::runtime_error("Pie7 missing physical blank row for mutant");
}
void pie7_reject(const Grid& good,const Pie& model,Work& work,auto mutate,std::string_view name) {
    pie7_need(pie7_geometry(good,model,work).empty(),"Pie7 physical mutant needs a good baseline");
    work.spend(good.cells.size()*(2u*sizeof(Cell)+32u)+4096u);
    auto bad=good; mutate(bad);
    pie7_need(!pie7_geometry(bad,model,work).empty(),name);
}
void pie7_model_reject(const Grid& good,const Pie& model,Work& work,auto mutate,std::string_view name) {
    pie7_need(pie7_geometry(good,model,work).empty(),"Pie7 Model mutant needs a good baseline");
    work.spend(model.slices.size()*1024u+4096u); auto bad=model; mutate(bad);
    pie7_need(!pie7_geometry(good,bad,work).empty(),name);
}
void pie7_write(std::string_view name,const json::JsonMutDoc& report) {
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto path=std::filesystem::path(LOOM_MERMAID_REPORT_DIR)/name;
    std::ofstream out(path,std::ios::binary|std::ios::trunc); ASSERT_TRUE(out); out<<report.to_string()<<'\n';
#else
    static_cast<void>(name); static_cast<void>(report);
#endif
}
void pie7_lifecycle_rejected(const Pie7Frames& good,Work& work,auto mutate,std::string_view name) {
    pie7_lifecycle(good,work);
    std::size_t cells=0;for(const auto& frame:good.views)cells+=frame.cells.size();
    work.spend(cells*(2u*sizeof(Cell)+32u)+sizeof(Pie7Frames)+4096u);
    auto bad=good;mutate(bad);
    bool rejected=false;
    try {pie7_lifecycle(bad,work);}
    catch(const std::exception& error) {if(pie7_resource(error.what()))throw;rejected=true;}
    pie7_need(rejected,name);
}
TEST(MermaidPie7, SubcellContractHasIndependentGoodGridAndMeaningfulPhysicalNegatives) {
    // This hand-authored grid derives only from 386/85/1 and capacity 40.
    // It proves the checker before any producer is changed or trusted.
    const Pie model{"Pets adopted by volunteers",false,{{"Dogs",386},{"Cats",85},{"Rats",1}}};
    const auto good=scene({"Pets adopted by volunteers",std::string("Dogs┃")+std::string(33,'#')+" 81.8%",std::string("Cats┃")+std::string(7,'*')+" 18.0%","Rats┃· 0.2% (1; below 0.5 cell)",kPieSubcellLegend,"",kSentinel,""});
    Work work{kPie7SceneLimit}; ASSERT_TRUE(pie7_geometry(good,model,work).empty())<<explain(pie7_geometry(good,model,work));
    for (const auto glyph : {"█","."}) pie7_reject(good,model,work,[&](Grid& bad){bad.edit(pie7_unique(bad,"·",work)).text=glyph;},"Pie7 dot glyph contract");
    for (const auto raw : {"0","2"}) pie7_reject(good,model,work,[&](Grid& bad){const auto p=pie7_unique(bad,"(1; below 0.5 cell)",work);write(bad,{p.x+1,p.y},raw);},"Pie7 physical exact raw value");
    pie7_reject(good,model,work,[&](Grid& bad){const auto p=pie7_unique(bad,"Rats",work);for(int x=0;x<bad.width;++x)bad.edit({x,p.y}).text=" ";},"Pie7 missing tiny owner");
    pie7_reject(good,model,work,[&](Grid& bad){const auto p=pie7_unique(bad,"Rats",work);write(bad,p,"Bats");},"Pie7 wrong tiny owner");
    pie7_reject(good,model,work,[&](Grid& bad){const auto p=pie7_unique(bad,"(1; below 0.5 cell)",work);for(int x=p.x;x<bad.width;++x)bad.edit({x,p.y}).text=" ";write(bad,{0,5},"(1; below 0.5 cell)");},"Pie7 annotation moved off owner row");
    pie7_reject(good,model,work,[&](Grid& bad){const auto p=pie7_unique(bad,kPieSubcellLegend,work);bad.edit({p.x+7,p.y}).text="3";},"Pie7 physical scale 40 mutation");
    pie7_reject(good,model,work,[&](Grid& bad){const auto p=pie7_unique(bad,kPieSubcellLegend,work);for(int x=0;x<bad.width;++x)bad.edit({x,p.y}).text=" ";},"Pie7 missing legend");
    pie7_reject(good,model,work,[&](Grid& bad){write(bad,{0,7},kPieSubcellLegend);},"Pie7 duplicate legend");
    pie7_reject(good,model,work,[&](Grid& bad){const auto p=pie7_unique(bad,"·",work),cats=pie7_unique(bad,"Cats",work);bad.edit(p).text=" ";bad.edit({p.x,cats.y}).text="·";},"Pie7 moved dot to Cats");
    pie7_reject(good,model,work,[&](Grid& bad){const auto p=pie7_unique(bad,"0.2%",work);write(bad,p,"0.0%");},"Pie7 exact share");
    pie7_reject(good,model,work,[&](Grid& bad){write(bad,{0,5},"X");},"Pie7 orphan ink");
    pie7_model_reject(good,model,work,[](Pie& bad){bad.slices.back().value=2;},"Pie7 source total changed");
    pie7_model_reject(good,model,work,[](Pie& bad){std::swap(bad.slices[0].label,bad.slices[2].label);},"Pie7 source owner exchange");
    // Both synthetic negatives call the same lifecycle entrypoint as actual
    // five-capture source scenes, with a charged copy of its frame bundle.
    work.spend(good.cells.size()*5u*(2u*sizeof(Cell)+32u)+sizeof(Pie7Frames)+4096u);
    const Pie7Frames frames{{good,good,good,good,good}};
    pie7_lifecycle_rejected(frames,work,[&](Pie7Frames& bad){bad.views[0].edit(pie7_unique(bad.views[0],kSentinel,work)).text=" ";},"Pie7 missing synthetic following token");
    pie7_lifecycle_rejected(frames,work,[](Pie7Frames& bad){bad.views[2].edit({0,0}).text="X";},"Pie7 wrong synthetic restored frame");
}
TEST(MermaidPie7, SevenCompleteOfficialSourcesRequireDrawingGeometryAndGoodFirstMutants) {
    const auto fixture_root=std::filesystem::path(__FILE__).parent_path().parent_path()/"fixtures/mermaid_native/pie7";
    Work setup; const auto raw=pie7_read(fixture_root/"pie7_native_authority.json",setup);
    ASSERT_EQ(digest(raw),"a3fb35a55dc325111e76ffa8a9b463c77eb89f9183e11047bd798e97f6b805df");
    const auto authority=json::parse(raw); ASSERT_TRUE(authority);
    ASSERT_EQ(authority->root().get_string("schema"),"pie7_whole_source_native_test_authority/v1");
    ASSERT_EQ(authority->root().get("cases").size(),7u);
    json::JsonMutDoc report; auto root=report.object(),records=root.make_arr();
    int complete=0,fallbacks=0,resources=0,errors=0;
    authority->root().get("cases").iter([&](JsonVal source_case) {
        Work work{kPie7SceneLimit}; auto row=root.make_obj(); std::string_view phase="source_read_before_capture";
        const int ordinal=static_cast<int>(source_case.get_int("ordinal")); row.set("ordinal",ordinal);
        try {
            const auto source=pie7_read(fixture_root/source_case.get_string("source_file"),work);
            phase="five_captures_and_publication_before_source_binding";
            const auto frames=pie7_observe(source,root,row,work); row.set("five_frames_published",true);
            phase="complete_source_binding";
            work.spend(source.size()*8u+8192u);
            pie7_need(source.size()==static_cast<std::size_t>(source_case.get_int("source_bytes"))&&digest(source)==source_case.get_string("source_sha256"),"Pie7 complete raw source identity");
            pie7_need(source==pie7_read(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/source_case.get_string("source_path"),work),"Pie7 source remains the vendored original");
            const auto prefix=source_case.get_string("complete_prefix");
            pie7_need(prefix.size()==static_cast<std::size_t>(source_case.get_int("prefix_bytes"))&&source.starts_with(prefix)&&digest(std::string_view(source).substr(prefix.size()))==source_case.get_string("body_sha256"),"Pie7 full source envelope/body partition");
            row.set("source_bound_after_publication",true); pie7_lifecycle(frames,work);
            phase="complete_Model_decode_after_source_binding";
            const auto encoded=source_case.get("Model"),canonical_value=source_case.get("Model_canonical_json");
            pie7_need(canonical_value.is_str(),"Pie7 canonical Model byte string");
            const auto canonical=canonical_value.as_str();work.spend(canonical.size()*(16u*sizeof(std::string)+128u)+8192u);
            pie7_need(digest(canonical)==source_case.get_string("Model_canonical_sha256"),"Pie7 unchanged source-design canonical Model SHA");
            const auto canonical_document=json::parse(canonical);pie7_need(canonical_document.has_value(),"Pie7 canonical Model JSON");
            const auto model=pie_model(canonical_document->root()),recorded=pie_model(encoded);
            pie7_need(model.title==recorded.title&&model.show_data==recorded.show_data&&model.slices.size()==recorded.slices.size(),"Pie7 canonical and recorded complete Model shape");
            for(std::size_t n=0;n<model.slices.size();++n)pie7_need(model.slices[n].label==recorded.slices[n].label&&exact(model.slices[n].value,recorded.slices[n].value),"Pie7 canonical and recorded full owner/value/order");
            pie7_need(model.slices.size()>=2u&&model.slices.size()<=7u,"Pie7 frozen complete slice count"); row.set("complete_Model_decoded",true);
            phase="mandatory_wide_drawing";
            if (same_source_surface(frames.views[0],frames.views[3])) {
                ++fallbacks; row.set("outcome","mandatory_source_fallback"); ADD_FAILURE()<<"Pie7 ordinal "<<ordinal<<" remains ordinary source";
            } else {
                phase="whole_geometry"; const auto good=pie7_geometry(frames.views[0],model,work); pie7_need(good.empty(),good.empty()?"Pie7 wide geometry":good.front());
                pie7_need(pie7_geometry(frames.views[2],model,work).empty(),"Pie7 restored geometry");
                if (!same_source_surface(frames.views[1],frames.views[4])) pie7_need(pie7_geometry(frames.views[1],model,work).empty(),"Pie7 narrow complete drawing");
                row.set("good_before_mutants",true); phase="good_first_mutations";
                pie7_model_reject(frames.views[0],model,work,[](Pie& bad){bad.slices.pop_back();},"Pie7 dropped owned slice");
                pie7_model_reject(frames.views[0],model,work,[](Pie& bad){bad.slices.front().value*=2;},"Pie7 changed common total");
                pie7_model_reject(frames.views[0],model,work,[](Pie& bad){std::swap(bad.slices.front().label,bad.slices.back().label);},"Pie7 exchanged owners");
                pie7_reject(frames.views[0],model,work,[&](Grid& bad){bad.edit({0,pie7_blank_row(bad,work)}).text="X";},"Pie7 actual orphan ink");
                // Mutate actual captures and ask the real five-frame
                // lifecycle validator to reject them; unequal bytes alone
                // cannot earn negative-test credit.
                pie7_lifecycle_rejected(frames,work,[&](Pie7Frames& bad){bad.views[0].edit(pie7_unique(bad.views[0],kSentinel,work)).text=" ";},"Pie7 actual following mutation must fail lifecycle");
                pie7_lifecycle_rejected(frames,work,[](Pie7Frames& bad){bad.views[2].edit({0,0}).text="X";},"Pie7 actual restored mutation must fail lifecycle");
                if (ordinal==12) {
                    for (const auto glyph : {"█","."}) pie7_reject(frames.views[0],model,work,[&](Grid& bad){bad.edit(pie7_unique(bad,"·",work)).text=glyph;},"Pie7 subcell glyph is not an ordinary bar or arbitrary dot");
                    for (const double wrong : {0.0,2.0}) pie7_model_reject(frames.views[0],model,work,[&](Pie& bad){bad.slices.back().value=wrong;},"Pie7 exact positive Rats raw value");
                    pie7_reject(frames.views[0],model,work,[&](Grid& bad){const auto p=pie7_unique(bad,"(1; below 0.5 cell)",work);write(bad,p,"(2; below 0.5 cell)");},"Pie7 physical raw value mutation");
                    pie7_reject(frames.views[0],model,work,[&](Grid& bad){const auto p=pie7_unique(bad,"0.2%",work);write(bad,p,"0.0%");},"Pie7 physical percentage mutation");
                    pie7_reject(frames.views[0],model,work,[&](Grid& bad){const auto p=pie7_unique(bad,"Rats",work);for(int x=0;x<bad.width;++x)bad.edit({x,p.y}).text=" ";},"Pie7 tiny owner row missing");
                    pie7_reject(frames.views[0],model,work,[&](Grid& bad){const auto p=pie7_unique(bad,kPieSubcellLegend,work);write(bad,p,"Scale: 400 cells = 100%; dot = positive share below 0.5 cell");},"Pie7 actual scale mutation");
                    pie7_reject(frames.views[0],model,work,[&](Grid& bad){const auto p=pie7_unique(bad,kPieSubcellLegend,work);for(int x=0;x<bad.width;++x)bad.edit({x,p.y}).text=" ";},"Pie7 missing scale legend");
                    pie7_reject(frames.views[0],model,work,[&](Grid& bad){write(bad,{0,pie7_blank_row(bad,work)},kPieSubcellLegend);},"Pie7 duplicate actual scale legend");
                    pie7_reject(frames.views[0],model,work,[&](Grid& bad){const auto p=pie7_unique(bad,"·",work),cats=pie7_unique(bad,"Cats",work);bad.edit(p).text=" ";bad.edit({p.x,cats.y}).text="·";},"Pie7 dot assigned to a large slice");
                    pie7_reject(frames.views[0],model,work,[&](Grid& bad){const auto p=pie7_unique(bad,"(1; below 0.5 cell)",work);for(int x=p.x;x<p.x+19;++x)bad.edit({x,p.y}).text=" ";},"Pie7 missing below-resolution annotation");
                    pie7_reject(frames.views[0],model,work,[&](Grid& bad){const auto p=pie7_unique(bad,"(1; below 0.5 cell)",work);for(int x=p.x;x<bad.width;++x)bad.edit({x,p.y}).text=" ";write(bad,{0,pie7_blank_row(bad,work)},"(1; below 0.5 cell)");},"Pie7 annotation moved to another actual row");
                }
                ++complete; row.set("outcome","complete_geometry_and_mutations");
            }
        } catch(const std::exception& error) {
            if (pie7_resource(error.what())) { ++resources; row.set("outcome","resource_error"); }
            else { ++errors; row.set("outcome","verification_error"); }
            row.set("error",error.what()); ADD_FAILURE()<<"Pie7 ordinal "<<ordinal<<" phase "<<phase<<": "<<error.what();
        }
        row.set("phase",std::string(phase)); row.set("work_remaining",static_cast<std::int64_t>(work.left)); records.append(row);
    });
    root.set("complete",complete); root.set("mandatory_source_fallbacks",fallbacks); root.set("resource_errors",resources); root.set("verification_errors",errors);
    root.set("formal_matrix_admissions",0); root.set("foreign_assertions_certified",0); root.add("cases",records); report.set_root(root); pie7_write("mermaid-pie7-whole-source-native.json",report);
    EXPECT_EQ(complete,7); EXPECT_EQ(fallbacks,0); EXPECT_EQ(resources,0); EXPECT_EQ(errors,0);
}

} // namespace

namespace {

// Supplementary Pie source controls. They are literal complete sources, not
// fixture ordinal/hash dispatch, and model construction follows all five
// public captures and complete source binding.
enum class Pie7SupplementKind { Positive, LegacyPositive, ValidUnsupported, LocalMalformed };
enum class Pie7SupplementModel { Inline, FoldedLf, FoldedCrlf, Tiny, LegacyTextPosition };
struct Pie7Supplement {
    std::string_view name, source, source_sha256;
    Pie7SupplementKind kind;
    Pie7SupplementModel model;
};
constexpr std::array<Pie7Supplement,15> kPie7Supplementary{{
    {"bare_inline_title",
     "pie title  Sports   in Sweden\n\"A\": 2\n\"B\": 1\n",
     "5cc900e5a4ffc7a0aa4c79716a84d521eca5d50eecae55af19344692c31e3f5f",Pie7SupplementKind::Positive,Pie7SupplementModel::Inline},
    {"folded_description_lf_opaque",
     "---\ndescription: >-\n  prose says config and title but is opaque metadata.\n---\npie title Folded LF\n\"A\": 4\n\"B\": 1\n",
     "0ac82c189882ea867dd25ad99e7e44ca0017297e2f607d923c6aeb8bf59287c7",Pie7SupplementKind::Positive,Pie7SupplementModel::FoldedLf},
    {"folded_description_crlf_opaque",
     "---\r\ndescription: >-\r\n  prose says config and title but is opaque metadata.\r\n---\r\npie title Folded CRLF\r\n\"A\": 4\r\n\"B\": 1\r\n",
     "012640b499128405a8fc00a023a95ba75ac20c62ad9b4e9721a73020f3af9336",Pie7SupplementKind::Positive,Pie7SupplementModel::FoldedCrlf},
    {"bare_tiny_1_9999",
     "pie title Tiny Ratios\n\"Large\": 9999\n\"Tiny\": 1\n",
     "455433aa7b8df6159ddd85610b25d22d1594211a3117052f178156801bcd7f89",Pie7SupplementKind::Positive,Pie7SupplementModel::Tiny},
    {"legacy_text_position_profile",
     "---\nconfig:\n  pie:\n    textPosition: 0.9\n---\npie\ntitle Existing textPosition profile\n\"A\": 2\n\"B\": 1\n",
     "0697b065c5e2da57af07c6844fb25a762e0a80e1c9439dd654bc2bd4dfd124c9",Pie7SupplementKind::LegacyPositive,Pie7SupplementModel::LegacyTextPosition},
    {"missing_description_fence",
     "---\ndescription: >-\n  missing root close\npie title Missing close\n\"A\": 1\n\"B\": 2\n",
     "d4e5b3fd08b32ddd55a3efe197578ac14bf867371a702a9f2f523447c9eea296",Pie7SupplementKind::LocalMalformed,Pie7SupplementModel::Inline},
    {"duplicate_description_root",
     "---\ndescription: >-\n  first description\ndescription: >-\n  duplicate root\n---\npie title Duplicate description\n\"A\": 1\n\"B\": 2\n",
     "32c77ef76f0313b5f15e53fc90d662fab9ba5d92cb6fe9546651097448469627",Pie7SupplementKind::LocalMalformed,Pie7SupplementModel::Inline},
    {"unknown_root_is_valid_unsupported",
     "---\ndescription: >-\n  opaque metadata\nunknown: retained\n---\npie title Unknown root\n\"A\": 1\n\"B\": 2\n",
     "c6564018c3fcf305fc171fc09f13bec729c8310466844c878580c24ba782e80c",Pie7SupplementKind::ValidUnsupported,Pie7SupplementModel::Inline},
    {"config_root_is_valid_unsupported",
     "---\ndescription: >-\n  opaque metadata\nconfig:\n  pie:\n    textPosition: 0.9\n---\npie title Config remains pending\n\"A\": 1\n\"B\": 2\n",
     "cf2c56f074b44ddda1c47ec844507521d05bb89f4c8f72170294bccbfef83cc4",Pie7SupplementKind::ValidUnsupported,Pie7SupplementModel::Inline},
    {"title_root_is_valid_unsupported",
     "---\ntitle: frontmatter title remains outside the Pie-only description profile\n---\npie title Root title\n\"A\": 1\n\"B\": 2\n",
     "64ba533b67725e5b6f9feee69f0b71cb1b5de375a5481aa42735ba3d2f4e9bf3",Pie7SupplementKind::ValidUnsupported,Pie7SupplementModel::Inline},
    {"one_space_payload_is_valid_unsupported",
     "---\ndescription: >-\n one physical payload space\n---\npie title Bad indentation\n\"A\": 1\n\"B\": 2\n",
     "01adf42374bee33b4b0afd6ec6a4b5f48baa350b43110e2e5b9446d2bbe5bb75",Pie7SupplementKind::ValidUnsupported,Pie7SupplementModel::Inline},
    {"missing_opening_fence",
     "description: >-\n  no opening fence\n---\npie title Missing opening\n\"A\": 1\n\"B\": 2\n",
     "7d73b14cc0a905bc7a10f214846a00e3272ae094dc555eaca3c219906f18b2c4",Pie7SupplementKind::LocalMalformed,Pie7SupplementModel::Inline},
    {"bare_cr_is_valid_unsupported",
     "pie title Bare CR\r\"A\": 1\n\"B\": 2\n",
     "dd2c77d4bf555da9dcbecab60e68e7e0324e36bc9bf4ba911d7304123731d612",Pie7SupplementKind::ValidUnsupported,Pie7SupplementModel::Inline},
    {"directive_prefix_is_valid_unsupported",
     "%%{init: {\"pie\": {\"textPosition\": 0.9}} }%%\npie title Directive prefix\n\"A\": 1\n\"B\": 2\n",
     "6d8e73fd8ee6c24b428198c9914086992afa1ec7f8ae3047f9596fab98478ec4",Pie7SupplementKind::ValidUnsupported,Pie7SupplementModel::Inline},
    {"folded_description_xy_is_valid_unsupported",
     "---\ndescription: >-\n  Pie-only metadata must not unlock an XY body.\n---\nxychart\nx-axis [A, B]\nbar [1, 2]\n",
     "cc814937d0090d850ece6f75939debe8920afbc63d90a5e9746671d206487015",Pie7SupplementKind::ValidUnsupported,Pie7SupplementModel::Inline},
}};

Pie pie7_supplementary_model(Pie7SupplementModel profile) {
    switch (profile) {
        case Pie7SupplementModel::Inline:
            return {"Sports in Sweden",false,{{"A",2},{"B",1}}};
        case Pie7SupplementModel::FoldedLf:
            return {"Folded LF",false,{{"A",4},{"B",1}}};
        case Pie7SupplementModel::FoldedCrlf:
            return {"Folded CRLF",false,{{"A",4},{"B",1}}};
        case Pie7SupplementModel::Tiny:
            return {"Tiny Ratios",false,{{"Large",9999},{"Tiny",1}}};
        case Pie7SupplementModel::LegacyTextPosition:
            return {"Existing textPosition profile",false,{{"A",2},{"B",1}}};
    }
    throw std::runtime_error("Pie7 supplementary Model profile");
}

TEST(MermaidPie7, SupplementarySourceControlsCloseEnvelopeAndSubcellBoundaries) {
    json::JsonMutDoc report; auto root=report.object(),records=root.make_arr();
    int complete=0,mandatory_fallbacks=0,legacy_complete=0,resources=0,errors=0,malformed=0,unsupported=0;
    for (const auto& control : kPie7Supplementary) {
        Work work{kPie7SceneLimit}; auto row=root.make_obj(); std::string_view phase="source_read_before_capture";
        row.set("name",control.name); row.set("class",control.kind == Pie7SupplementKind::Positive ? "mandatory_positive" :
            (control.kind == Pie7SupplementKind::LegacyPositive ? "legacy_complete_regression" :
             (control.kind == Pie7SupplementKind::ValidUnsupported ? "valid_unsupported" : "local_malformed")));
        try {
            phase="five_captures_and_publication_before_source_binding";
            const auto frames=pie7_observe(control.source,root,row,work); row.set("five_frames_published",true);
            phase="complete_source_binding";
            work.spend(control.source.size()*8u+8192u);
            pie7_need(digest(control.source)==control.source_sha256,"Pie7 supplementary complete literal source SHA");
            row.set("source_bound_after_publication",true); pie7_lifecycle(frames,work);
            const bool ordinary=same_source_surface(frames.views[0],frames.views[3]);
            if (control.kind != Pie7SupplementKind::Positive && control.kind != Pie7SupplementKind::LegacyPositive) {
                pie7_need(ordinary,"Pie7 supplementary rejected source must remain whole ordinary source");
                row.set("model_admitted",false);
                row.set("outcome",control.kind == Pie7SupplementKind::LocalMalformed ? "local_malformed_whole_fallback" : "valid_unsupported_whole_fallback");
                if (control.kind == Pie7SupplementKind::LocalMalformed) ++malformed; else ++unsupported;
            } else {
                phase="complete_Model_decode_after_source_binding";
                work.spend(4u*sizeof(Slice)+4u*sizeof(std::string)+4096u);
                const auto model=pie7_supplementary_model(control.model); row.set("complete_Model_decoded",true);
                phase="mandatory_wide_drawing";
                if (ordinary) {
                    if (control.kind == Pie7SupplementKind::LegacyPositive)
                        row.set("outcome","legacy_profile_source_fallback");
                    else {
                        ++mandatory_fallbacks; row.set("outcome","mandatory_source_fallback");
                    }
                    ADD_FAILURE()<<"Pie7 supplementary "<<control.name<<" remains ordinary source";
                } else {
                    phase="whole_geometry";
                    const auto good=pie7_geometry(frames.views[0],model,work);
                    pie7_need(good.empty(),good.empty()?"Pie7 supplementary wide geometry":good.front());
                    pie7_need(pie7_geometry(frames.views[2],model,work).empty(),"Pie7 supplementary restored geometry");
                    if (!same_source_surface(frames.views[1],frames.views[4]))
                        pie7_need(pie7_geometry(frames.views[1],model,work).empty(),"Pie7 supplementary narrow geometry");
                    if (control.model == Pie7SupplementModel::Tiny) {
                        pie7_need(find_text(frames.views[0],"0.0%",work).size()==1u,"Pie7 tiny share rounds to visible 0.0%");
                        pie7_need(find_text(frames.views[0],"(1; below 0.5 cell)",work).size()==1u,"Pie7 tiny raw value remains visible");
                    }
                    row.set("good_before_mutants",true); phase="good_first_mutations";
                    pie7_model_reject(frames.views[0],model,work,[](Pie& bad){bad.slices.pop_back();},"Pie7 supplementary dropped owner");
                    pie7_reject(frames.views[0],model,work,[&](Grid& bad) {
                        const auto p=pie7_unique(bad,model.slices.front().label,work);
                        for (int x=0;x<bad.width;++x) bad.edit({x,p.y}).text=" ";
                    },"Pie7 supplementary physical owner removal");
                    pie7_lifecycle_rejected(frames,work,[&](Pie7Frames& bad) {
                        bad.views[0].edit(pie7_unique(bad.views[0],kSentinel,work)).text=" ";
                    },"Pie7 supplementary lifecycle following mutation");
                    if (control.model == Pie7SupplementModel::Tiny) {
                        pie7_model_reject(frames.views[0],model,work,[](Pie& bad){bad.slices.back().value=2;},"Pie7 tiny raw Model mutation");
                        pie7_reject(frames.views[0],model,work,[&](Grid& bad) {
                            const auto p=pie7_unique(bad,"(1; below 0.5 cell)",work);
                            write(bad,p,"(2; below 0.5 cell)");
                        },"Pie7 tiny raw caption mutation");
                    }
                    ++complete;
                    if (control.kind == Pie7SupplementKind::LegacyPositive) ++legacy_complete;
                    row.set("outcome",control.kind == Pie7SupplementKind::LegacyPositive ?
                        "legacy_complete_geometry_and_mutations" : "complete_geometry_and_mutations");
                }
            }
        } catch(const std::exception& error) {
            if (pie7_resource(error.what())) { ++resources; row.set("outcome","resource_error"); }
            else { ++errors; row.set("outcome","verification_error"); }
            row.set("error",error.what()); ADD_FAILURE()<<"Pie7 supplementary "<<control.name<<" phase "<<phase<<": "<<error.what();
        }
        row.set("phase",std::string(phase)); row.set("work_remaining",static_cast<std::int64_t>(work.left)); records.append(row);
    }
    root.set("complete",complete); root.set("mandatory_source_fallbacks",mandatory_fallbacks);
    root.set("legacy_complete_regressions",legacy_complete);
    root.set("local_malformed_whole_fallbacks",malformed); root.set("valid_unsupported_whole_fallbacks",unsupported);
    root.set("resource_errors",resources); root.set("verification_errors",errors);
    root.set("formal_matrix_admissions",0); root.set("foreign_assertions_certified",0); root.add("cases",records);
    report.set_root(root); pie7_write("mermaid-pie7-supplementary-source-controls.json",report);
    EXPECT_EQ(complete,5); EXPECT_EQ(mandatory_fallbacks,0); EXPECT_EQ(legacy_complete,1);
    EXPECT_EQ(malformed,3); EXPECT_EQ(unsupported,7);
    EXPECT_EQ(resources,0); EXPECT_EQ(errors,0);
}

} // namespace
namespace {
// Directed reverse-domain XY102 test-only admission. This block is deliberately
// EOF-local: it neither changes the generic ascending Scale nor gives a new
// public test/production seam. It captures complete sources before opening the
// source/facts/assertion/Model authority used to evaluate the unchanged producer.

constexpr std::string_view kReverse3AuthoritySha =
    "760fb25adcafea732d5dc52003c26b699f633f77e882a6496aa2e0e8a35ab4d3";
constexpr std::string_view kReverse3FactsSha =
    "eeb3c1508b25202838fb175fac8757e4428d2d41e76a0901b6373b04a3f3a3a8";
constexpr std::string_view kReverse3CasesSha =
    "89ce2ef6ad779481285ce61b771a64fd059b17a824eb2c292934ff13393562db";
constexpr std::string_view kReverse3UpstreamSha =
    "7b9cdcd4a97d448b9e637e77ff5b15cd900562583f4c8c580048aae77d7c74bd";
constexpr std::string_view kReverse3AssertionSha =
    "533970a6d958eba6ce093e9140741b2acd64ccc25f873fdc2f7db394d1cf2bd8";

struct Reverse3Source {
    std::string_view id,source_sha,source_asset,model_path,model_sha,reference_path,reference_sha;
    std::size_t source_bytes = 0;
    double x_first,x_last,y_first,y_last;
    bool inferred_x = false,bar = false,clipped_last = false;
    std::string_view x_title,y_title;
};
constexpr std::array<Reverse3Source,3> kReverse3Sources{{
    {"merman/8b87d96bf3d739887fc7",
     "8b87d96bf3d739887fc7b181411a1383abd6e0278dea7246d859cdf044a227ae",
     "sources/8b87d96bf3d739887fc7.mmd",
     "runtime-capture/oracle-models/27be21ff981962ca18d73a9b7a43234d24b64a7ce0ca028ce8dae9a4256b8c01.json",
     "27be21ff981962ca18d73a9b7a43234d24b64a7ce0ca028ce8dae9a4256b8c01",
     "runtime-capture/reference-outputs/b94cc5923c9835371a583896b3584cc2b7e8b460e737220c099239a9bc9f075f.txt",
     "b94cc5923c9835371a583896b3584cc2b7e8b460e737220c099239a9bc9f075f",57u,
     45.5,1.34,45.5,33.0,false,false,true,"",""},
    {"merman/95713fa70777f7bda028",
     "95713fa70777f7bda028a846b3c19e2eada414c4ab8a10dd4240998eb65ab21b",
     "sources/95713fa70777f7bda028.mmd",
     "runtime-capture/oracle-models/b3ed46b1f497389b9114460c05a7dc4c35fa6bc0751dfde312a50cb48b078b6d.json",
     "b3ed46b1f497389b9114460c05a7dc4c35fa6bc0751dfde312a50cb48b078b6d",
     "runtime-capture/reference-outputs/8dc6a39e2c759e178180d9facc72dea56ad7613e6399af7c24b2487aa63bef1b.txt",
     "8dc6a39e2c759e178180d9facc72dea56ad7613e6399af7c24b2487aa63bef1b",80u,
     45.5,0.34,45.5,0.33,false,false,false,"xAxisName","yAxisName"},
    {"merman/f6e1865fc70d2c098e9c",
     "f6e1865fc70d2c098e9ccd2d249a2631237074a3b893d18df6296371de9a96fb",
     "sources/f6e1865fc70d2c098e9c.mmd",
     "runtime-capture/oracle-models/2b231cce31563bd2300c00b69bd34ea2e2611dbf0b30ca0f449236c268494c57.json",
     "2b231cce31563bd2300c00b69bd34ea2e2611dbf0b30ca0f449236c268494c57",
     "runtime-capture/reference-outputs/0bb96d1bd8ec8736881f3b82823f32d6480e3db4d1bc7878363d61e8c7f4f603.txt",
     "0bb96d1bd8ec8736881f3b82823f32d6480e3db4d1bc7878363d61e8c7f4f603",35u,
     1.0,1.0,45.5,33.0,true,true,true,"",""},
}};

constexpr std::size_t kReverse3SceneLimit = 128000000u;
constexpr int kReverse3MaxCaptureHeight = 64;
struct Reverse3Frames {
    Grid wide,plain_wide,narrow,plain_narrow,restored;
};
struct Reverse3CaptureLedger {
    int captured = 0, published = 0, height = 0;
    std::size_t opening_work = 0;
};
bool reverse3_resource(std::string_view error) {
    return error.find("work exhausted") != std::string_view::npos ||
        error.find("budget") != std::string_view::npos ||
        error.find("bounds") != std::string_view::npos;
}
void reverse3_need(bool condition,std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}
std::string reverse3_read(const std::filesystem::path& path,Work& work) {
    const auto bytes = std::filesystem::file_size(path);
    reverse3_need(bytes <= 65536u,"Reverse3 immutable source/model asset exceeds 64 KiB");
    // Before stream/string construction reserve both the byte buffer and a
    // bounded integrity pass. The native fixture has no new public Work API.
    work.spend(static_cast<std::size_t>(bytes)*4u+512u);
    std::ifstream input(path,std::ios::binary);
    reverse3_need(input.good(),"Reverse3 immutable asset cannot be opened");
    std::string value{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    reverse3_need(value.size()==bytes,"Reverse3 immutable asset changed while read");
    return value;
}
std::size_t reverse3_source_construction_reserve(std::size_t source_bytes) {
    reverse3_need(source_bytes <= 65536u,"Reverse3 source exceeds bounded capture input");
    return (source_bytes+128u)*(32u*sizeof(std::string)+256u)+16384u;
}
std::size_t reverse3_decimal_width(std::size_t value) {
    std::size_t width=1u; while(value>=10u) { value/=10u; ++width; } return width;
}
std::size_t reverse3_packed_cell_bytes(const Grid& grid) {
    bounded(grid);std::size_t bytes=0u;
    for(const auto& cell:grid.cells) for(const std::string_view field:{std::string_view(cell.text),std::string_view(cell.foreground),std::string_view(cell.background)}) {
        reverse3_need(field.size()<=65536u,"Reverse3 cell field exceeds bounded publication");
        const auto add=reverse3_decimal_width(field.size())+1u+field.size();
        reverse3_need(bytes<=std::numeric_limits<std::size_t>::max()-add,"Reverse3 publication size overflow");
        bytes+=add;
    }
    return bytes;
}
void reverse3_append_field(std::string& output,std::string_view field) {
    char number[32];const auto converted=std::to_chars(std::begin(number),std::end(number),field.size());
    reverse3_need(converted.ec==std::errc{},"Reverse3 field length encoding");
    output.append(number,converted.ptr);output.push_back(':');output.append(field);
}
void reverse3_publish(const Grid& grid,std::string_view name,json::JsonMutVal root,json::JsonMutVal row,Work& work,Reverse3CaptureLedger& ledger) {
    bounded(grid);
    // Pay the three-field byte/count scan before inspecting any retained Cell.
    work.spend(grid.cells.size()*3u+1024u);
    const auto bytes=reverse3_packed_cell_bytes(grid);
    // One owned packed string and its Json copy preserve every PixelAt text,
    // foreground, and background triple without an uncharged per-pixel tree.
    reverse3_need(bytes<=(std::numeric_limits<std::size_t>::max()-4u*sizeof(std::string)-8u*sizeof(json::JsonMutVal)-4096u)/2u,
                  "Reverse3 publication charge overflow");
    work.spend(bytes*2u+4u*sizeof(std::string)+8u*sizeof(json::JsonMutVal)+4096u);
    std::string packed;packed.reserve(bytes);
    for(const auto& cell:grid.cells) {
        reverse3_append_field(packed,cell.text);reverse3_append_field(packed,cell.foreground);reverse3_append_field(packed,cell.background);
    }
    reverse3_need(packed.size()==bytes,"Reverse3 packed color-cell publication changed while built");
    auto frame=root.make_obj();frame.set("width",grid.width);frame.set("height",grid.height);
    frame.set("cell_count",static_cast<int>(grid.cells.size()));
    frame.set("cell_encoding","row-major decimal-byte-length:text:foreground:background/v1");
    frame.add("color_cells",root.make_str(packed));row.add(name,frame);++ledger.published;
}
Reverse3Frames reverse3_capture_before_decode(std::string_view source,Work& work,json::JsonMutVal root,json::JsonMutVal row,Reverse3CaptureLedger& ledger) {
    // This is a scene-private finite ledger. It pays the two fenced strings
    // before element construction, capture_paid pays Pixel/Cell copies before
    // each Screen allocation, and reverse3_publish pays its retained report.
    work.spend(reverse3_source_construction_reserve(source.size()));
    auto drawing=loom::ui::render_markdown(fenced(source,"mermaid"));
    auto ordinary=loom::ui::render_markdown(fenced(source,"text"));
    drawing->ComputeRequirement();ordinary->ComputeRequirement();
    const int height=std::max(32,std::max(drawing->requirement().min_y,ordinary->requirement().min_y)+12);
    reverse3_need(height>0&&height<=kReverse3MaxCaptureHeight,"Reverse3 source-derived capture height exceeds local scene bound");
    ledger.height=height;
    Reverse3Frames frames;
    constexpr std::array<int,5> widths{{240,240,40,40,240}};
    constexpr std::array<std::string_view,5> names{{"wide_240","source_control_240","narrow_40","source_control_40","restored_240"}};
    for(std::size_t n=0;n<widths.size();++n) {
        Grid frame=capture_paid(n==0u||n==2u||n==4u?drawing:ordinary,widths[n],height,work);++ledger.captured;
        if(n==0u) frames.wide=std::move(frame);
        else if(n==1u) frames.plain_wide=std::move(frame);
        else if(n==2u) frames.narrow=std::move(frame);
        else if(n==3u) frames.plain_narrow=std::move(frame);
        else frames.restored=std::move(frame);
        const Grid& published=n==0u?frames.wide:n==1u?frames.plain_wide:n==2u?frames.narrow:n==3u?frames.plain_narrow:frames.restored;
        reverse3_publish(published,names[n],root,row,work,ledger);
    }
    return frames;
}
Reverse3Frames reverse3_capture_before_decode(std::string_view source,Work& work) {
    // Closed fallback inputs use a discarded report tree, retaining the same
    // PixelAt acquisition, exact color serialization, and source-height contract.
    json::JsonMutDoc discard;auto root=discard.object(),row=root.make_obj();Reverse3CaptureLedger ledger;
    return reverse3_capture_before_decode(source,work,root,row,ledger);
}
int reverse3_signed_row(double value,double first,double last,const Axes& axis) {
    if (!std::isfinite(value) || !std::isfinite(first) || !std::isfinite(last) || first == last)
        throw std::runtime_error("Reverse3 signed y projection is not finite");
    const auto unit = std::clamp((value-first)/(last-first),0.0,1.0);
    return static_cast<int>(std::round(axis.y+(axis.top-axis.y)*unit));
}
std::string reverse3_token(double value) { return numeric_x_label(value); }
std::optional<Point> reverse3_role(const Grid& grid,std::string_view text,Work& work,auto predicate) {
    const auto positions = find_text(grid,text,work);
    std::optional<Point> result;
    for(const auto point:positions) {
        work.spend();
        if(!predicate(point)) continue;
        if(result) return {};
        result=point;
    }
    return result;
}
void reverse3_claim_axis(const Grid& grid,const Axes& axis,Claims& claims,Work& work) {
    for (int y=axis.top;y<=axis.y;++y) {
        work.spend();
        reverse3_need((stroke(grid.at({axis.x,y}).text).bits & (U|D)) != 0 || y==axis.top || y==axis.y,
                      "Reverse3 y axis lost a reciprocal physical segment");
        claims.cell({axis.x,y});
    }
    for (int x=axis.x;x<=axis.right;++x) {
        work.spend();
        reverse3_need((stroke(grid.at({x,axis.y}).text).bits & (L|R)) != 0 || x==axis.x || x==axis.right,
                      "Reverse3 x axis lost a reciprocal physical segment");
        claims.cell({x,axis.y});
    }
}
bool reverse3_data_mark(const Cell& cell) {
    return fill(cell.text) || stroke(cell.text).bits != 0 || cell.text=="●" || cell.text=="○" ||
        cell.text=="◉" || cell.text=="⊕" || braille_composite(cell.text);
}
bool reverse3_reciprocal(const Grid& grid,Point from,Point to) {
    const Point step{to.x-from.x,to.y-from.y};
    const int forward = step.x==1 ? R : step.x==-1 ? L : step.y==1 ? D : U;
    const int reverse = forward==R ? L : forward==L ? R : forward==D ? U : D;
    return (stroke(grid.at(from).text).bits & forward) && (stroke(grid.at(to).text).bits & reverse);
}
bool reverse3_connect_line(const Grid& grid,Point start,Point end,const Axes& axis,Claims& claims,Work& work) {
    const auto index = [&](Point p) { return static_cast<std::size_t>(p.y*grid.width+p.x); };
    const auto cells=grid.cells.size();
    work.spend(cells*(sizeof(int)+sizeof(Point)+2u*sizeof(std::size_t)+32u)+4096u);
    std::vector<int> parent(cells,-1);std::vector<Point> queue{start};
    parent[index(start)]=static_cast<int>(index(start));
    constexpr std::array<Point,4> steps{{{0,-1},{1,0},{0,1},{-1,0}}};
    for(std::size_t at=0;at<queue.size();++at) {
        const auto here=queue[at];if(here==end) break;
        for(const auto step:steps) {
            work.spend();const Point next{here.x+step.x,here.y+step.y};
            if(!grid.has(next)||next.x<axis.x||next.x>axis.right||next.y<axis.top||next.y>axis.y) continue;
            // The source endpoint itself may meet the y axis. A route may not
            // travel along or return through that axis to borrow its stroke.
            if(next.x==axis.x && next!=start && next!=end) continue;
            const auto slot=index(next);if(parent[slot]>=0||!reverse3_data_mark(grid.at(next))||!reverse3_reciprocal(grid,here,next)) continue;
            parent[slot]=static_cast<int>(index(here));queue.push_back(next);
        }
    }
    if(parent[index(end)]<0)return false;
    for(int current=static_cast<int>(index(end));;) {
        work.spend();claims.data({current%grid.width,current/grid.width},0,work);
        if(current==parent[static_cast<std::size_t>(current)])break;
        current=parent[static_cast<std::size_t>(current)];
    }
    return true;
}
std::vector<std::string> reverse3_model_errors(const MermanXY& model,const Reverse3Source& source,Work& work) {
    std::vector<std::string> errors;
    work.spend(4096u+model.points.size()*1024u);
    if(model.ownership.horizontal||model.unknown_config||!model.show_title||model.show_values||model.outside_values)
        errors.push_back("Reverse3 profile changed orientation/configuration visibility");
    if(!model.x_policy.labels||!model.x_policy.title||!model.x_policy.ticks||!model.x_policy.line||
       !model.y_policy.labels||!model.y_policy.title||!model.y_policy.ticks||!model.y_policy.line)
        errors.push_back("Reverse3 profile lost a complete axis role");
    if(!model.authored_y||!exact(model.authored_y->first,source.y_first)||!exact(model.authored_y->second,source.y_last)||
       !(source.y_first>source.y_last)||!exact(model.y_first,source.y_first)||!exact(model.y_last,source.y_last))
        errors.push_back("Reverse3 Model sorted or lost authored y order");
    if(model.ownership.category_title!=source.x_title||model.ownership.value_title!=source.y_title)
        errors.push_back("Reverse3 Model changed an axis-title role");
    if(model.ownership.series.size()!=1u||model.points.size()!=1u||model.named_series.size()!=1u||model.named_series.front()) {
        errors.push_back("Reverse3 Model changed the one unnamed dataset");
        return errors;
    }
    const auto& series=model.ownership.series.front();const auto& points=model.points.front();
    if((series.type=="bar")!=source.bar||series.values.empty()||series.values.size()!=points.size()) {
        errors.push_back("Reverse3 Model changed dataset kind/cardinality");
        return errors;
    }
    for(std::size_t n=0;n<points.size();++n) {
        work.spend();
        if(!points[n].value||!exact(*points[n].value,series.values[n]))
            errors.push_back("Reverse3 Model point/value relation changed");
    }
    bool clipped_last=false;
    for(const auto& series:model.ownership.series) for(const auto value:series.values) {
        work.spend();
        if(value < source.y_last) clipped_last=true;
        if(value > source.y_first) errors.push_back("Reverse3 Model requires unsupported authored-first-side clipping");
    }
    if(clipped_last!=source.clipped_last) errors.push_back("Reverse3 Model/reference clip-last fact changed");
    if(source.inferred_x) {
        // The pinned complete parser Model infers a linear singleton [1,1].
        // Absence of an authored range does not turn its axis into a band.
        if(model.band||model.authored_x||!model.category_names.empty()||
           !exact(model.x_first,1)||!exact(model.x_last,1)||points.size()!=1u||!exact(points.front().x,1))
            errors.push_back("Reverse3 inferred linear singleton x changed kind/domain/coordinate");
    } else if(model.band||!model.authored_x||!exact(model.authored_x->first,source.x_first)||
              !exact(model.authored_x->second,source.x_last)||!(source.x_first>source.x_last)||
              !exact(model.x_first,source.x_first)||!exact(model.x_last,source.x_last)) {
        errors.push_back("Reverse3 Model sorted or lost authored x order");
    }
    return errors;
}
std::vector<std::string> reverse3_geometry(const Grid& grid,const MermanXY& model,const Reverse3Source& source,Work& work) {
    std::vector<std::string> errors;
    try {
        if(model.ownership.series.size()!=1u||model.points.size()!=1u||model.named_series.size()!=1u||
           model.ownership.series.front().values.empty()||model.ownership.series.front().values.size()!=model.points.front().size())
            return {"Reverse3 geometry rejected malformed Model before indexing"};
        bounded(grid);
        // Claims and its owner mask must be paid before their vector allocation.
        work.spend(grid.cells.size()*(sizeof(bool)+sizeof(std::uint32_t)+2u*sizeof(std::size_t)+32u)+8192u);
        Claims claims(grid); claims.enable_data(work);
        auto candidates=axes(grid,work);
        // A data-line elbow can form an L without being an authored axis.
        // Select only a complete pair with the source's physical endpoint ticks;
        // all other data/axis-looking cells remain subject to ownership below.
        const auto first_token=reverse3_token(source.x_first),last_token=reverse3_token(source.x_last);
        const auto first_positions=find_text(grid,first_token,work),last_positions=find_text(grid,last_token,work);
        std::erase_if(candidates,[&](const Axes& candidate) {
            const auto endpoint=[&](const auto& positions,const std::string& token,int x) {
                return std::ranges::any_of(positions,[&](Point p) {
                    work.spend();const int center=p.x+(ftxui::string_width(token)-1)/2;
                    return p.y==candidate.y+1&&center==x&&(stroke(grid.at({x,candidate.y}).text).bits&D);
                });
            };
            if(source.inferred_x) return !std::ranges::any_of(first_positions,[&](Point p) {
                work.spend();const int center=p.x+(ftxui::string_width(first_token)-1)/2;
                return p.y==candidate.y+1&&center>candidate.x&&center<=candidate.right&&
                    (stroke(grid.at({center,candidate.y}).text).bits&D);
            });
            return !endpoint(first_positions,first_token,candidate.x)||!endpoint(last_positions,last_token,candidate.right);
        });
        if(candidates.size()!=1u) return {"Reverse3 requires one complete source-ticked physical axis pair"};
        const auto axis=candidates.front();
        if(!(axis.top<axis.y&&axis.x<axis.right)) return {"Reverse3 axis bounds invalid"};
        reverse3_claim_axis(grid,axis,claims,work);

        // Five source-ordered ticks map first->physical bottom, last->physical
        // top. This intentionally does not call Scale, whose sort is wrong here.
        for(int n=0;n<5;++n) {
            const double value=n==4 ? source.y_last : source.y_first+(source.y_last-source.y_first)*(n/4.0);
            const auto text=reverse3_token(value);const auto point=reverse3_role(grid,text,work,[&](Point p) {
                return p.y>=axis.top&&p.y<=axis.y&&p.x+ftxui::string_width(text)<=axis.x;
            });
            if(!point) { errors.push_back("Reverse3 y tick token missing/ambiguous"); continue; }
            const int expected=reverse3_signed_row(value,source.y_first,source.y_last,axis);
            if(point->x>=axis.x||point->y!=expected) errors.push_back("Reverse3 signed y tick physical direction changed");
            if(!(stroke(grid.at({axis.x,expected}).text).bits&L)) errors.push_back("Reverse3 authored y tick lost its physical left arm");
            claims.text(*point,text);
        }
        const auto xfirst_text=reverse3_token(source.x_first),xlast_text=reverse3_token(source.x_last);
        const auto center=[](Point p,const std::string& text) { return p.x+(ftxui::string_width(text)-1)/2; };
        const auto xfirst=reverse3_role(grid,xfirst_text,work,[&](Point p) {
            return p.y>axis.y&&(source.inferred_x||center(p,xfirst_text)==axis.x);
        });
        const auto xlast=source.inferred_x ? xfirst : reverse3_role(grid,xlast_text,work,[&](Point p) {
            return p.y>axis.y&&center(p,xlast_text)==axis.right;
        });
        if(!xfirst||!xlast||xfirst->y<=axis.y||xlast->y<=axis.y) errors.push_back("Reverse3 numeric x endpoint ticks missing");
        else {
            if(!source.inferred_x&&(center(*xfirst,xfirst_text)!=axis.x||center(*xlast,xlast_text)!=axis.right))
                errors.push_back("Reverse3 x endpoint labels moved inward from their physical endpoints");
            claims.text(*xfirst,xfirst_text);if(!source.inferred_x) claims.text(*xlast,xlast_text);
        }
        if(!source.x_title.empty()) {
            const auto title=reverse3_role(grid,source.x_title,work,[&](Point p) { return p.y>axis.y; });
            if(!title||title->y<=axis.y) errors.push_back("Reverse3 x-axis title missing/wrong role");
            else claims.text(*title,source.x_title);
        }
        if(!source.y_title.empty()) {
            const auto title=reverse3_role(grid,source.y_title,work,[&](Point p) { return p.x<axis.x; });
            if(!title||title->x>=axis.x) errors.push_back("Reverse3 y-axis title missing/wrong role");
            else claims.text(*title,source.y_title);
        }

        const auto& values=model.ownership.series.front().values;
        const auto& points=model.points.front();
        work.spend(values.size()*(sizeof(Point)+2u*sizeof(std::size_t)+32u)+128u);
        std::vector<Point> samples;samples.reserve(values.size());
        for(std::size_t n=0;n<values.size();++n) {
            work.spend();
            const double authored_x=source.inferred_x ? source.x_first : points[n].x;
            const auto token=reverse3_token(authored_x);
            const bool first_coordinate=exact(authored_x,source.x_first);
            const bool last_coordinate=exact(authored_x,source.x_last);
            if(!source.inferred_x&&!first_coordinate&&!last_coordinate) {
                errors.push_back("Reverse3 source coordinate is outside the authored endpoint profile"); continue;
            }
            const int expected_x=first_coordinate ? axis.x : axis.right;
            const auto tick=reverse3_role(grid,token,work,[&](Point p) {
                return p.y>axis.y&&(source.inferred_x||p.x+(ftxui::string_width(token)-1)/2==expected_x);
            });
            if(!tick) { errors.push_back("Reverse3 source coordinate has no endpoint physical tick"); continue; }
            const int x=tick->x+(ftxui::string_width(token)-1)/2;
            const int y=reverse3_signed_row(values[n],source.y_first,source.y_last,axis);
            const Point sample{x,y};
            if(!grid.has(sample)||!reverse3_data_mark(grid.at(sample))) errors.push_back("Reverse3 source point/clipped boundary missing");
            else samples.push_back(sample);
        }
        if(source.bar) {
            if(samples.size()!=1u) errors.push_back("Reverse3 singleton bar sample missing");
            else {
                const int baseline=reverse3_signed_row(source.y_first,source.y_first,source.y_last,axis);
                const int endpoint=reverse3_signed_row(values.front(),source.y_first,source.y_last,axis);
                for(int y=std::min(baseline,endpoint);y<=std::max(baseline,endpoint);++y) {
                    work.spend();const Point cell{samples.front().x,y};
                    if(y==baseline) { claims.cell(cell); continue; }
                    if(!fill(grid.at(cell).text)) errors.push_back("Reverse3 bar lacks a baseline-to-boundary owned cell");
                    else claims.data(cell,0,work);
                }
            }
        } else if(samples.size()==1u) {
            claims.data(samples.front(),0,work);
        } else if(samples.size()==2u && !reverse3_connect_line(grid,samples[0],samples[1],axis,claims,work)) {
            errors.push_back("Reverse3 line endpoints lack one source-ordered reciprocal path");
        }
        claims.sentinel(work);
        if(!claims.all_claimed(work)) errors.push_back("Reverse3 contains unowned/non-source chart ink");
    } catch(const std::exception& error) {
        if(reverse3_resource(error.what())) throw;
        errors.push_back(error.what());
    }
    return errors;
}
void reverse3_require_fallback(std::string_view source,Work& work) {
    const auto frames=reverse3_capture_before_decode(source,work);
    reverse3_need(same_source_surface(frames.wide,frames.plain_wide),"Reverse3 closed control painted a partial wide chart");
    reverse3_need(same_source_surface(frames.narrow,frames.plain_narrow),"Reverse3 closed control painted a partial narrow chart");
    reverse3_need(frames.restored==frames.wide,"Reverse3 closed control changed restoration");
}
struct Reverse3SyntheticProfile {
    MermanXY model;
    double x_first = 0,x_last = 0,y_first = 0,y_last = 0;
    bool inferred_x = false,bar = false,clipped_last = false;
};
Reverse3Source reverse3_synthetic_source(const Reverse3SyntheticProfile& profile,std::string_view name) {
    return {name,"","","","","","",0u,profile.x_first,profile.x_last,profile.y_first,profile.y_last,
            profile.inferred_x,profile.bar,profile.clipped_last,profile.model.ownership.category_title,profile.model.ownership.value_title};
}
Reverse3SyntheticProfile reverse3_synthetic_model_binding(JsonVal root,std::string_view name,Work& work) {
    work.spend(4096u);
    reverse3_need(root.is_obj()&&root.get_string("orientation")=="vertical","Reverse3 synthetic Model orientation");
    const auto series=root.get("series"),x=root.get("x"),y=root.get("y");
    reverse3_need(series.is_obj()&&x.is_obj()&&y.is_obj(),"Reverse3 synthetic Model object closure");
    const auto values=series.get("values"),coordinates=series.get("x"),clipped=root.get("clip_last");
    reverse3_need(values.is_arr()&&coordinates.is_arr()&&clipped.is_arr()&&values.size()==coordinates.size()&&values.size()==clipped.size()&&values.size()>0u,
                  "Reverse3 synthetic Model sample closure");
    reverse3_need(y.get("first").is_num()&&y.get("last").is_num()&&y.get("first").as_double()>y.get("last").as_double(),
                  "Reverse3 synthetic Model must retain directed y endpoints");
    const bool bar=series.get_string("type")=="bar";
    reverse3_need((bar?series.get_string("name")=="Bar 1":series.get_string("name")=="Line 1"),"Reverse3 synthetic owner identity");
    Reverse3SyntheticProfile profile;
    profile.x_first=x.get("first").as_double();profile.x_last=x.get("last").as_double();profile.y_first=y.get("first").as_double();profile.y_last=y.get("last").as_double();
    profile.inferred_x=!x.get("numeric").as_bool();profile.bar=bar;
    profile.model.band=false;profile.model.ownership.horizontal=false;
    profile.model.authored_y={{profile.y_first,profile.y_last}};profile.model.y_first=profile.y_first;profile.model.y_last=profile.y_last;
    profile.model.ownership.category_title=x.get_string("title");profile.model.ownership.value_title=y.get_string("title");
    profile.model.ownership.series.push_back({series.get_string("type"),series.get_string("name"),{}});profile.model.named_series.push_back(false);
    auto& owned=profile.model.ownership.series.front();std::vector<MermanPoint> points;
    for(std::size_t n=0;n<values.size();++n) {
        work.spend();reverse3_need(values.at(n).is_num()&&coordinates.at(n).is_num()&&clipped.at(n).is_bool(),"Reverse3 synthetic scalar closure");
        const auto value=values.at(n).as_double(),coordinate=coordinates.at(n).as_double();
        reverse3_need(std::isfinite(value)&&std::isfinite(coordinate),"Reverse3 synthetic finite scalar closure");
        owned.values.push_back(value);points.push_back({coordinate,value});
        if(clipped.at(n).as_bool()) profile.clipped_last=true;
    }
    if(name=="p1_endpoints"||name=="p2_zero") {
        reverse3_need(!bar&&x.get("numeric").as_bool()&&profile.x_first>profile.x_last&&values.size()==2u&&!profile.clipped_last,
                      "Reverse3 synthetic signed line Model");
        profile.model.band=false;profile.model.authored_x={{profile.x_first,profile.x_last}};
        profile.model.x_first=profile.x_first;profile.model.x_last=profile.x_last;profile.model.points.push_back(std::move(points));
    } else if(name=="p3_clipped_bar") {
        reverse3_need(bar&&!x.get("numeric").as_bool()&&exact(profile.x_first,1)&&exact(profile.x_last,1)&&
                      values.size()==1u&&profile.clipped_last,"Reverse3 synthetic singleton clipped bar Model");
        profile.model.band=false;profile.model.x_first=1;profile.model.x_last=1;
        profile.model.points.push_back({{1.0,owned.values.front()}});
    } else throw std::runtime_error("Reverse3 unknown synthetic Model name");
    const auto source=reverse3_synthetic_source(profile,name);const auto errors=reverse3_model_errors(profile.model,source,work);
    reverse3_need(errors.empty(),explain(errors));return profile;
}

void reverse3_write(std::string_view name,const json::JsonMutDoc& report) {
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto path=std::filesystem::path(LOOM_MERMAID_REPORT_DIR)/name;
    std::ofstream output(path,std::ios::binary|std::ios::trunc);
    ASSERT_TRUE(output);output<<report.to_pretty_string()<<'\n';
#else
    static_cast<void>(name);static_cast<void>(report);
#endif
}
void reverse3_bind_source(JsonVal row,JsonVal fact,const Reverse3Source& source,Work& work) {
    work.spend(2048u);
    reverse3_need(row.get_string("id")==source.id,"Reverse3 selected input id changed");
    reverse3_need(row.get_string("sha256")==source.source_sha&&digest(row.get_string("input"))==source.source_sha,
                  "Reverse3 selected complete input changed");
    reverse3_need(fact.get_string("id")==source.id&&fact.get_string("input_sha256")==source.source_sha,
                  "Reverse3 selected fact/source binding changed");
    const auto y=fact.get("explicit_axis_domains").get("y-axis");
    reverse3_need(y.is_arr()&&y.size()==2u&&exact(y.at(0).as_double(),source.y_first)&&exact(y.at(1).as_double(),source.y_last),
                  "Reverse3 selected authored y order changed");
    if(source.inferred_x) reverse3_need(!fact.get("explicit_axis_domains").get("x-axis"),"Reverse3 inferred x became explicit");
    else {
        const auto x=fact.get("explicit_axis_domains").get("x-axis");
        reverse3_need(x.is_arr()&&x.size()==2u&&exact(x.at(0).as_double(),source.x_first)&&exact(x.at(1).as_double(),source.x_last),
                      "Reverse3 selected authored x order changed");
    }
    const auto reference=fact.get("primary_model_reference");
    reverse3_need(reference.get_string("path")==source.model_path&&reference.get_string("sha256")==source.model_sha,
                  "Reverse3 whole Model selection changed");
}
MermanXY reverse3_handmade_model(bool bar=false,bool titles=false) {
    MermanXY result;result.band=false;result.ownership.horizontal=false;
    result.authored_y={{2,-2}};result.y_first=2;result.y_last=-2;
    result.ownership.category_title=titles?"xAxisName":"";result.ownership.value_title=titles?"yAxisName":"";
    result.ownership.series.push_back({bar?"bar":"line",bar?"Bar 1":"Line 1",bar?std::vector<double>{-3}:std::vector<double>{1,-1}});
    result.named_series.push_back(false);
    if(bar) {result.x_first=1;result.x_last=1;result.points={{{1,-3}}};}
    else {result.authored_x={{2,-2}};result.x_first=2;result.x_last=-2;result.points={{{2,1},{-2,-1}}};}
    return result;
}
Grid reverse3_handmade_line(bool titles=false) {
    Grid grid{34,27,std::vector<Cell>(34u*27u)};
    constexpr int left=8,top=1,bottom=21,first=left,last=28;
    for(int y=top;y<=bottom;++y) grid.edit({left,y}).text="│";
    for(int x=left;x<=28;++x) grid.edit({x,bottom}).text="─";
    for(int n=0;n<5;++n) {
        const int y=bottom-5*n;grid.edit({left,y}).text=n?"┤":"┼";
        write(grid,{0,y},reverse3_token(2-n));
    }
    grid.edit({last,bottom}).text="┬";
    write(grid,{first,bottom+1},"2");write(grid,{last,bottom+1},"-2");
    // Source-order L path (2,1)->(-2,-1), never an extrema-sorted line.
    for(int x=first;x<=20;++x) grid.edit({x,16}).text="─";
    grid.edit({first,16}).text="┼"; // Preserve y-tick, y-axis, and source data arms.
    for(int y=6;y<=16;++y) grid.edit({20,y}).text="│";
    for(int x=20;x<=last;++x) grid.edit({x,6}).text="─";
    grid.edit({20,16}).text="╯";grid.edit({20,6}).text="╭";
    if(titles) {write(grid,{0,0},"yAxisName");write(grid,{0,bottom+2},"xAxisName");}
    write(grid,{0,25},kSentinel);return grid;
}

Grid reverse3_handmade_zero_line() {
    Grid grid{34,27,std::vector<Cell>(34u*27u)};
    constexpr int left=8,top=1,bottom=21,first=left,last=28;
    for(int y=top;y<=bottom;++y) grid.edit({left,y}).text="│";
    for(int x=left;x<=28;++x) grid.edit({x,bottom}).text="─";
    constexpr std::array<std::string_view,5> labels{{"1","0.5","0","-0.5","-1"}};
    for(int n=0;n<5;++n) { const int y=bottom-5*n;grid.edit({left,y}).text=n?"┤":"┼";write(grid,{0,y},labels[static_cast<std::size_t>(n)]); }
    grid.edit({last,bottom}).text="┬";
    write(grid,{first,bottom+1},"1");write(grid,{last,bottom+1},"-1");
    for(int x=first;x<=last;++x) grid.edit({x,11}).text="─";
    grid.edit({first,11}).text="┼"; // Preserve y-tick, y-axis, and zero data arms.
    write(grid,{0,25},kSentinel);return grid;
}

Grid reverse3_handmade_bar() {
    Grid grid{34,27,std::vector<Cell>(34u*27u)};
    constexpr int left=8,top=1,bottom=21,slot=14;
    for(int y=top;y<=bottom;++y) grid.edit({left,y}).text="│";
    for(int x=left;x<=28;++x) grid.edit({x,bottom}).text="─";
    for(int n=0;n<5;++n) {
        const int y=bottom-5*n;grid.edit({left,y}).text=n?"┤":"┼";
        write(grid,{0,y},reverse3_token(3-0.5*n));
    }
    grid.edit({slot,bottom}).text="┬";write(grid,{slot,bottom+1},"1");
    for(int y=top;y<bottom;++y) grid.edit({slot,y}).text="█";
    write(grid,{0,25},kSentinel);return grid;
}
TEST(MermaidChartsReverseDomain3Oracle, IndependentSignedAxisGoodGridsAndMeaningfulMutations) {
    auto line=reverse3_handmade_line();auto line_model=reverse3_handmade_model();
    Reverse3Source line_source{"hand/line","","","","","","",0u,2,-2,2,-2,false,false,false,"",""};
    Work good_work;ASSERT_TRUE(reverse3_model_errors(line_model,line_source,good_work).empty());
    const auto line_errors=reverse3_geometry(line,line_model,line_source,good_work);ASSERT_TRUE(line_errors.empty())<<explain(line_errors);
    // Both axes may share a multi-cell numeric endpoint. A centered X label
    // starts left of the Y axis, but remains outside the Y tick role.
    auto shared_numeric=line;auto shared_model=line_model;
    shared_model.authored_x={{200,-200}};shared_model.x_first=200;shared_model.x_last=-200;
    shared_model.authored_y={{200,-200}};shared_model.y_first=200;shared_model.y_last=-200;
    shared_model.ownership.series.front().values={100,-100};shared_model.points={{{200,100},{-200,-100}}};
    for(int n=0;n<5;++n) write(shared_numeric,{0,21-5*n},reverse3_token(200-100*n));
    write(shared_numeric,{7,22},"200");write(shared_numeric,{27,22},"-200");
    Reverse3Source shared_source{"hand/shared_numeric","","","","","","",0u,200,-200,200,-200,false,false,false,"",""};
    Work shared_model_work;ASSERT_TRUE(reverse3_model_errors(shared_model,shared_source,shared_model_work).empty());
    Work shared_grid_work;const auto shared_errors=reverse3_geometry(shared_numeric,shared_model,shared_source,shared_grid_work);
    ASSERT_TRUE(shared_errors.empty())<<explain(shared_errors);
    auto adjacent_ticks=shared_numeric;
    for(int n=0;n<5;++n) {
        const int row=21-5*n;const auto token=reverse3_token(200-100*n);
        for(int x=0;x<8;++x) adjacent_ticks.edit({x,row}).text=" ";
        write(adjacent_ticks,{8-ftxui::string_width(token),row},token);
    }
    Work adjacent_work;const auto adjacent_errors=reverse3_geometry(adjacent_ticks,shared_model,shared_source,adjacent_work);
    ASSERT_TRUE(adjacent_errors.empty())<<explain(adjacent_errors);

    auto titled=reverse3_handmade_line(true);auto titled_model=reverse3_handmade_model(false,true);
    auto titled_source=line_source;titled_source.x_title="xAxisName";titled_source.y_title="yAxisName";
    Work titled_work;const auto titled_errors=reverse3_geometry(titled,titled_model,titled_source,titled_work);ASSERT_TRUE(titled_errors.empty())<<explain(titled_errors);
    auto zero_grid=reverse3_handmade_zero_line();auto zero_model=line_model;
    zero_model.authored_x={{1,-1}};zero_model.x_first=1;zero_model.x_last=-1;zero_model.authored_y={{1,-1}};zero_model.y_first=1;zero_model.y_last=-1;
    zero_model.ownership.series.front().values={0,0};zero_model.points={{{1,0},{-1,0}}};
    Reverse3Source zero_source{"hand/zero","","","","","","",0u,1,-1,1,-1,false,false,false,"",""};
    Work zero_good;ASSERT_TRUE(reverse3_model_errors(zero_model,zero_source,zero_good).empty());
    const auto zero_errors=reverse3_geometry(zero_grid,zero_model,zero_source,zero_good);ASSERT_TRUE(zero_errors.empty())<<explain(zero_errors);
    auto bar=reverse3_handmade_bar();auto bar_model=reverse3_handmade_model(true);
    bar_model.ownership.series.front().values={0};bar_model.points={{{1,0}}};
    Reverse3Source bar_source{"hand/bar","","","","","","",0u,1,1,3,1,true,true,true,"",""};
    bar_model.authored_y={{3,1}};bar_model.y_first=3;bar_model.y_last=1;
    Work bar_model_work;ASSERT_TRUE(reverse3_model_errors(bar_model,bar_source,bar_model_work).empty());
    Work bar_work;const auto bar_errors=reverse3_geometry(bar,bar_model,bar_source,bar_work);
    ASSERT_TRUE(bar_errors.empty())<<explain(bar_errors);
    auto forged_band=bar_model;forged_band.band=true;forged_band.category_names={"1"};Work forged_band_work;
    EXPECT_FALSE(reverse3_model_errors(forged_band,bar_source,forged_band_work).empty());

    auto reject_line=[&](Grid good,auto mutate,std::string_view name) {
        Work baseline;ASSERT_TRUE(reverse3_geometry(good,line_model,line_source,baseline).empty());
        mutate(good);Work check;EXPECT_FALSE(reverse3_geometry(good,line_model,line_source,check).empty())<<name;
    };
    auto reject_bar=[&](Grid good,auto mutate,std::string_view name) {
        Work baseline;ASSERT_TRUE(reverse3_geometry(good,bar_model,bar_source,baseline).empty());
        mutate(good);Work check;EXPECT_FALSE(reverse3_geometry(good,bar_model,bar_source,check).empty())<<name;
    };
    reject_line(line,[](Grid& g){write(g,{0,1},"2 ");write(g,{0,21},"-2");},"swapped signed y labels");
    reject_line(line,[](Grid& g){write(g,{8,22},"-2");write(g,{28,22},"2 ");},"swapped reverse x labels");
    reject_line(line,[](Grid& g){write(g,{8,22}," ");write(g,{9,22},"2");},"inward first endpoint label kept order");
    reject_line(line,[](Grid& g){write(g,{28,22},"  ");write(g,{27,22},"-2");},"inward last endpoint label kept order");
    reject_line(line,[](Grid& g){g.edit({14,16}).text=" ";},"missing owned line route cell");
    reject_line(line,[](Grid& g){g.edit({8,16}).text="├";},"authored y label retained but physical tick left arm erased");
    reject_bar(bar,[](Grid& g){g.edit({14,11}).text=" ";},"bar hole");
    { auto missing=titled;write(missing,{0,0},"wrongTitle");Work check;EXPECT_FALSE(reverse3_geometry(missing,titled_model,titled_source,check).empty())<<"removed y title"; }
    auto ascending=line_model;ascending.authored_y={{-2,2}};ascending.y_first=-2;ascending.y_last=2;Work model_work;
    EXPECT_FALSE(reverse3_model_errors(ascending,line_source,model_work).empty())<<"ordered domain mutation must reject before geometry";
    reject_line(line,[](Grid& g){g.edit({30,10}).text="█";},"unowned extra data-looking ink");
    auto lifecycle=line;write(lifecycle,{0,25},"MISSING");Work lifecycle_work;
    EXPECT_FALSE(reverse3_geometry(lifecycle,line_model,line_source,lifecycle_work).empty())<<"missing following sentinel";
    auto no_series=line_model;no_series.ownership.series.clear();no_series.points.clear();no_series.named_series.clear();Work no_series_work;
    EXPECT_FALSE(reverse3_model_errors(no_series,line_source,no_series_work).empty())<<"no-series Model must reject before geometry";
    EXPECT_FALSE(reverse3_geometry(line,no_series,line_source,no_series_work).empty())<<"geometry must not index an absent series";
    auto ragged=line_model;ragged.points.front().pop_back();Work ragged_work;
    EXPECT_FALSE(reverse3_model_errors(ragged,line_source,ragged_work).empty())<<"unequal Model cardinality must reject before indexing";
    EXPECT_FALSE(reverse3_geometry(line,ragged,line_source,ragged_work).empty())<<"geometry must guard unequal cardinality";
    auto missing_value=line_model;missing_value.points.front().front().value.reset();Work missing_value_work;
    EXPECT_FALSE(reverse3_model_errors(missing_value,line_source,missing_value_work).empty())<<"missing projected index must reject semantically";
    Work zero{0};EXPECT_THROW((void)reverse3_geometry(line,line_model,line_source,zero),std::runtime_error);
    Work short_work{line.cells.size()*(sizeof(bool)+sizeof(std::uint32_t)+2u*sizeof(std::size_t)+32u)+8191u};EXPECT_THROW((void)reverse3_geometry(line,line_model,line_source,short_work),std::runtime_error);
}
TEST(MermaidChartsReverseDomain3, ThreeBoundSourcesCapturePublishBeforeBindingAndRemainMandatory) {
    const auto fixtures=std::filesystem::path(LOOM_TESTS_DIR)/"fixtures";
    const auto bundle=fixtures/"mermaid_native/xy102_reverse3";
    Work assets;const auto authority=reverse3_read(bundle/"authority.json",assets);
    ASSERT_EQ(digest(authority),kReverse3AuthoritySha);ASSERT_TRUE(json::parse(authority));
    int complete=0,fallbacks=0,resources=0,verification=0;
    json::JsonMutDoc report;auto root=report.object(),rows=root.make_arr();
    root.set("kind","xy102_directed_reverse_domain3_test_only");
    root.set("upstream_smoke_callsites",1);root.set("upstream_smoke_runtime_applications",3);
    root.set("native_geometry_controls_additional",true);root.set("closed_profile_fallback_is_unmet_positive",true);
    root.set("scene_work_limit",static_cast<int>(kReverse3SceneLimit));root.set("scene_height_limit",kReverse3MaxCaptureHeight);
    struct Observed {
        std::string raw;Reverse3Frames frames;Work work;Reverse3CaptureLedger ledger;json::JsonMutVal row;
        bool captured = false;
    };
    std::array<Observed,3> observed;
    // This phase deliberately knows only the fixed source asset tuple. It
    // acquires, captures, and publishes every view before facts, cases, the
    // original assertion, or any parser Model is opened/decoded/bound.
    for(std::size_t n=0;n<kReverse3Sources.size();++n) {
        const auto& source=kReverse3Sources[n];auto& item=observed[n];item.work=Work{kReverse3SceneLimit};item.ledger.opening_work=item.work.left;
        item.row=root.make_obj();item.row.set("id",std::string(source.id));item.row.set("source_sha256",std::string(source.source_sha));
        std::string phase="acquire_complete_source";
        try {
            item.raw=reverse3_read(bundle/std::string(source.source_asset),item.work);
            reverse3_need(item.raw.size()==source.source_bytes&&digest(item.raw)==source.source_sha,"Reverse3 copied complete source bytes changed");
            phase="five_captures_and_publication_before_expectation_binding";
            item.frames=reverse3_capture_before_decode(item.raw,item.work,root,item.row,item.ledger);item.captured=true;
            item.row.set("capture_phase","published_before_source_facts_model_binding");item.row.set("captured_frames",item.ledger.captured);
            item.row.set("published_frames",item.ledger.published);item.row.set("capture_height",item.ledger.height);
            item.row.set("work_after_publication",static_cast<std::int64_t>(item.work.left));
            reverse3_need(item.ledger.captured==5&&item.ledger.published==5,"Reverse3 did not publish all five actual color-cell frames");
            reverse3_need(item.frames.restored==item.frames.wide,"Reverse3 source-wide restoration changed");
        } catch(const std::exception& error) {
            item.row.set("phase",phase);item.row.set("captured_frames",item.ledger.captured);item.row.set("published_frames",item.ledger.published);
            item.row.set("capture_height",item.ledger.height);item.row.set("work_remaining",static_cast<std::int64_t>(item.work.left));item.row.set("error",error.what());
            if(reverse3_resource(error.what())) {++resources;item.row.set("outcome","resource_error");}
            else {++verification;item.row.set("outcome","verification_error");ADD_FAILURE()<<"Reverse3 "<<source.id<<" "<<phase<<": "<<error.what();}
        }
        rows.append(item.row);
    }
    // Publication is durable before any facts, case catalogue, assertion, or
    // Model authority opens. Later phases mutate the same attached row handles.
    root.set("phase","published_before_expectation_binding");
    root.set("complete",0);root.set("mandatory_source_fallbacks",0);root.set("resource_errors",resources);root.set("verification_errors",verification);
    root.add("cases",rows);report.set_root(root);reverse3_write("mermaid-merman-xy102-reverse3-test-only.json",report);
    // All three source-frame records now exist. Only this second phase opens
    // the immutable cases/facts/Model authority and binds selected identities.
    const auto facts_raw=read(fixtures/"mermaid_native/merman_xy102_facts.json");
    const auto cases_raw=read(fixtures/"mermaid_terminal/merman/cases.json");
    ASSERT_EQ(digest(facts_raw),kReverse3FactsSha);ASSERT_EQ(digest(cases_raw),kReverse3CasesSha);
    const auto facts=json::parse(facts_raw),cases=json::parse(cases_raw);ASSERT_TRUE(facts&&cases);
    std::map<std::string,JsonVal> case_by_id;cases->root().get("cases").iter([&](JsonVal value) {
        if(value.get_string("family")=="xychart") reverse3_need(case_by_id.emplace(value.get_string("id"),value).second,"Reverse3 duplicate vendor xychart id");
    });
    std::map<std::string,JsonVal> fact_by_id;facts->root().get("cases").iter([&](JsonVal value) {
        if(value.get_string("id").starts_with("merman/")) reverse3_need(fact_by_id.emplace(value.get_string("id"),value).second,"Reverse3 duplicate facts id");
    });
    ASSERT_EQ(case_by_id.size(),102u);ASSERT_EQ(fact_by_id.size(),102u);
    for(std::size_t n=0;n<kReverse3Sources.size();++n) {
        const auto& source=kReverse3Sources[n];auto& item=observed[n];if(!item.captured) continue;
        std::string phase="source_facts_binding_after_publication";
        try {
            reverse3_need(case_by_id.contains(std::string(source.id))&&fact_by_id.contains(std::string(source.id)),"Reverse3 selected source missing");
            const auto input=case_by_id.at(std::string(source.id));const auto fact=fact_by_id.at(std::string(source.id));
            reverse3_need(input.get_string("input")==item.raw,"Reverse3 local complete source does not equal immutable selected input");
            reverse3_bind_source(input,fact,source,item.work);
            const auto assertion=reverse3_read(fixtures/"mermaid_terminal/merman/runtime-capture/assertions"/(std::string(kReverse3AssertionSha)+".rs"),item.work);
            reverse3_need(digest(assertion)==kReverse3AssertionSha&&digest(read(fixtures/"mermaid_terminal/merman/upstream/tests/xychart_model.rs"))==kReverse3UpstreamSha,
                          "Reverse3 original smoke assertion provenance changed");
            phase="model_decode_after_published_captures";const auto vendor=fixtures/"mermaid_terminal/merman";
            const auto model_raw=reverse3_read(vendor/std::string(source.model_path),item.work);
            const auto reference_raw=reverse3_read(vendor/std::string(source.reference_path),item.work);
            reverse3_need(digest(model_raw)==source.model_sha&&digest(reference_raw)==source.reference_sha,
                          "Reverse3 complete Model/reference bytes changed");
            reverse3_need((reference_raw.find("clipped=true")!=std::string::npos)==source.clipped_last,
                          "Reverse3 frozen reference clip fact changed");
            const auto parsed=json::parse(model_raw);reverse3_need(static_cast<bool>(parsed),"Reverse3 Model JSON parse");
            const auto model=merman_model(parsed->root(),fact);const auto model_errors=reverse3_model_errors(model,source,item.work);
            reverse3_need(model_errors.empty(),explain(model_errors));
            const bool fallback=same_source_surface(item.frames.wide,item.frames.plain_wide);
            const bool narrow_source=same_source_surface(item.frames.narrow,item.frames.plain_narrow);
            if(fallback) { ++fallbacks;ADD_FAILURE()<<"Reverse3 mandatory complete source retained as text: "<<source.id;item.row.set("outcome","mandatory_complete_source_fallback"); }
            else {
                phase="independent_signed_axis_geometry";const auto geometry=reverse3_geometry(item.frames.wide,model,source,item.work);
                reverse3_need(geometry.empty(),explain(geometry));
                if(!narrow_source) reverse3_need(reverse3_geometry(item.frames.narrow,model,source,item.work).empty(),
                                                 "Reverse3 narrow drawing is not a complete signed grid");
                ++complete;item.row.set("outcome","complete_signed_geometry");
            }
            item.row.set("narrow","source_fallback_or_complete_grid");item.row.set("work_remaining",static_cast<std::int64_t>(item.work.left));
        } catch(const std::exception& error) {
            item.row.set("phase",phase);item.row.set("work_remaining",static_cast<std::int64_t>(item.work.left));item.row.set("error",error.what());
            if(reverse3_resource(error.what())) {++resources;item.row.set("outcome","resource_error");}
            else {++verification;item.row.set("outcome","verification_error");ADD_FAILURE()<<"Reverse3 "<<source.id<<" "<<phase<<": "<<error.what();}
        }
    }
    root.set("phase","final_after_expectation_binding");
    root.set("complete",complete);root.set("mandatory_source_fallbacks",fallbacks);root.set("resource_errors",resources);root.set("verification_errors",verification);
    report.set_root(root);reverse3_write("mermaid-merman-xy102-reverse3-test-only.json",report);
    EXPECT_EQ(complete,3);EXPECT_EQ(fallbacks,0);EXPECT_EQ(resources,0);EXPECT_EQ(verification,0);
}
TEST(MermaidChartsReverseDomain3, RealMultilineSyntheticSourcesReachTheSameGeometryAndClosedFallbackControls) {
    const auto root=std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_native/xy102_reverse3";
    struct Positive { std::string_view name,source_sha,model_sha; };
    constexpr std::array<Positive,3> positives{{
        {"p1_endpoints","8b051323274e3a6a58ef1860416495092e3bc97faa33878ac056d732fd49de12","d87c4795eb0322331fda77ed9b8a5790ec952dd51667812cbd4c53def2574517"},
        {"p2_zero","945673029cfdef28ee62e06204f7f5ebaf8b232d0547e1c59278c5c8d9c86687","978a3091d692c88c3fcdafc865bc40ee0297a21a58aeef8016ada5fed52e0de2"},
        {"p3_clipped_bar","d91ac362122c815320fa709cb1fd18693d9a3ee9269362e98618ee9d83eef347","3849c34fc895820572c130b60336ac892cdea8a730f492002b9058dd013cb38b"},
    }};
    int positive_good=0;
    for(const auto& positive:positives) {
        SCOPED_TRACE(std::string(positive.name));
        Work work{kReverse3SceneLimit};
        try {
            const auto source=reverse3_read(root/"synthetic"/(std::string(positive.name)+".mmd"),work);
            const bool source_bound=digest(source)==positive.source_sha&&source.find('\n')!=std::string::npos;
            EXPECT_TRUE(source_bound)<<"Synthetic source must retain its hash and physical Mermaid newlines";
            if(!source_bound) continue;
            // Capture is before Model asset open/decode, just as with the complete
            // source rows. A local discard doc keeps the full color-frame ledger.
            const auto frames=reverse3_capture_before_decode(source,work);
            const auto model_raw=reverse3_read(root/"synthetic"/(std::string(positive.name)+".model.json"),work);
            const bool model_bound=digest(model_raw)==positive.model_sha;
            EXPECT_TRUE(model_bound);
            if(!model_bound) continue;
            const auto decoded=json::parse(model_raw);
            EXPECT_TRUE(decoded)<<"Synthetic source Model is complete JSON";
            if(!decoded) continue;
            const auto profile=reverse3_synthetic_model_binding(decoded->root(),positive.name,work);
            const auto profile_source=reverse3_synthetic_source(profile,positive.name);
            const bool drawn=!same_source_surface(frames.wide,frames.plain_wide);
            EXPECT_TRUE(drawn)<<"Mandatory synthetic reverse source fell back";
            EXPECT_EQ(frames.restored,frames.wide);
            if(!drawn||frames.restored!=frames.wide) continue;
            const auto wide_errors=reverse3_geometry(frames.wide,profile.model,profile_source,work);
            EXPECT_TRUE(wide_errors.empty())<<"Synthetic wide grid must satisfy signed ownership";
            if(!wide_errors.empty()) continue;
            if(!same_source_surface(frames.narrow,frames.plain_narrow)) {
                const auto narrow_errors=reverse3_geometry(frames.narrow,profile.model,profile_source,work);
                EXPECT_TRUE(narrow_errors.empty())<<"Synthetic narrow grid must be complete when drawn";
                if(!narrow_errors.empty()) continue;
            }
            auto malformed=profile.model;malformed.points.front().clear();Work malformed_work;
            EXPECT_FALSE(reverse3_model_errors(malformed,profile_source,malformed_work).empty())
                <<"Synthetic Model cardinality mutation must reject before geometry";
            auto missing_sentinel=frames.wide;Work locate;const auto markers=find_text(missing_sentinel,kSentinel,locate);
            EXPECT_EQ(markers.size(),1u)<<"Synthetic good grid must have one following-token lifecycle marker";
            if(markers.size()!=1u) continue;
            write(missing_sentinel,markers.front(),"MISSING");Work grid_mutant_work;
            EXPECT_FALSE(reverse3_geometry(missing_sentinel,profile.model,profile_source,grid_mutant_work).empty())
                <<"Synthetic Grid lifecycle mutation must reject after a good geometry pass";
            ++positive_good;
        } catch(const std::exception& error) {
            ADD_FAILURE()<<"Synthetic reverse source "<<positive.name<<": "<<error.what();
        }
    }
    EXPECT_EQ(positive_good,3);

    constexpr std::array<std::pair<std::string_view,std::string_view>,5> closed{{
        {"n1_degenerate_y","fb444cd8da23b1268fe377bfa7ed83a7a04e0f244b5f06384870ef18b6df2841"},
        {"n2_degenerate_numeric_x","d95677bead795da532846b17326dd5f53b13bc13612b1ada2a89cb0df03a6b31"},
        {"n3_nonfinite_axis","d04412a16abb02c4cc5298478e0e1b8f33ff950bfa7ab2dd4c59664e895d06cc"},
        {"n4_nonfinite_sample","05ca9b6b2a198b658aafcd0e075ac9f4c9f1022781168f9dfc9fb60afc529685"},
        {"n5_horizontal_reverse","b11e94812778b5403e8a283303734a8cf767ee02112ed6c2b18e212b43029729"},
    }};
    int closed_checked=0;
    for(const auto& [name,sha]:closed) {
        SCOPED_TRACE(std::string(name));
        try {
            Work work{kReverse3SceneLimit};const auto source=reverse3_read(root/"synthetic"/(std::string(name)+".mmd"),work);
            const bool source_bound=digest(source)==sha&&source.find('\n')!=std::string::npos;
            EXPECT_TRUE(source_bound);
            if(!source_bound) continue;
            EXPECT_NO_THROW(reverse3_require_fallback(source,work))<<name;
            ++closed_checked;
        } catch(const std::exception& error) {
            ADD_FAILURE()<<"Closed Reverse3 control "<<name<<": "<<error.what();
        }
    }
    EXPECT_EQ(closed_checked,5);
}
TEST(MermaidChartsReverseDomain3, ResourceBoundariesRunIndependentlyOfSyntheticDrawSuccess) {
    const auto root=std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_native/xy102_reverse3";
    try {
        Work load{kReverse3SceneLimit};const auto source=reverse3_read(root/"synthetic/p1_endpoints.mmd",load);
        const bool source_bound=digest(source)=="8b051323274e3a6a58ef1860416495092e3bc97faa33878ac056d732fd49de12";
        EXPECT_TRUE(source_bound);
        if(!source_bound) return;
        Work exhausted{0};EXPECT_THROW((void)reverse3_capture_before_decode(source,exhausted),std::runtime_error);
        Work one_before_frame{reverse3_source_construction_reserve(source.size())};
        EXPECT_THROW((void)reverse3_capture_before_decode(source,one_before_frame),std::runtime_error);
    } catch(const std::exception& error) {
        ADD_FAILURE()<<"Reverse3 independent resource controls: "<<error.what();
    }
}


} // namespace
