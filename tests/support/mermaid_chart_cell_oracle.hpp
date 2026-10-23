// Test-private Chart/Pie cell oracle mechanically extracted from
// test_mermaid_charts_semantics.cpp. No renderer, fixture selection, report,
// or admission policy belongs here.
#pragma once
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
import std;
import loom.serdes.json;
namespace loom::test::mermaid_chart {
namespace json = loom::utils::json;
using json::JsonVal;
inline constexpr int kMaxWidth = 240, kMaxHeight = 256;
inline constexpr std::string_view kDefaultFollowingToken = "AFTER_CHART_SEMANTICS";
struct Point { int x = 0, y = 0; bool operator==(const Point&) const = default; };
struct Cell {
    std::string text = " ", foreground, background;
    bool operator==(const Cell&) const = default;
};
struct Grid {
    int width = 0, height = 0;
    std::vector<Cell> cells;
    bool has(Point p) const { return p.x >= 0 && p.y >= 0 && p.x < width && p.y < height; }
    const Cell& at(Point p) const {
        static const Cell outside;
        return has(p) ? cells.at(static_cast<std::size_t>(p.y * width + p.x)) : outside;
    }
    Cell& edit(Point p) { return cells.at(static_cast<std::size_t>(p.y * width + p.x)); }
    std::string row(int y) const {
        std::string s;
        for (int x = 0; x < width; ++x) s += at({x,y}).text;
        return s;
    }
    std::string text() const {
        std::string s;
        for (int y = 0; y < height; ++y) s += row(y) + '\n';
        return s;
    }
    bool operator==(const Grid&) const = default;
};
struct Work {
    std::size_t left = 8000000;
    void spend(std::size_t n = 1) { if (left < n) throw std::runtime_error("Chart cell-oracle work exhausted"); left -= n; }
    void product(std::size_t count,std::size_t unit) { if (unit && count > left/unit) { left = 0; throw std::runtime_error("Chart cell-oracle work exhausted"); } spend(count*unit); }
};
inline void bounded(const Grid& g) {
    if (g.width <= 0 || g.height <= 0 || g.width > kMaxWidth || g.height > kMaxHeight ||
        g.cells.size() != static_cast<std::size_t>(g.width * g.height))
        throw std::runtime_error("Chart oracle capture bounds exceeded");
}
inline bool same_source_surface(const Grid& a,const Grid& b) {
    if (a.width != b.width || a.height != b.height) return false;
    bounded(a); bounded(b);
    // Languages may choose different token colors for identical code. This
    // predicate identifies a preserved source surface, never chart geometry.
    for (std::size_t n = 0; n < a.cells.size(); ++n)
        if (a.cells[n].text != b.cells[n].text || a.cells[n].background != b.cells[n].background) return false;
    return true;
}
inline Grid capture(ftxui::Element element,int width,int height) {
    if (width > kMaxWidth || height > kMaxHeight) throw std::runtime_error("Chart capture bounds exceeded");
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),ftxui::Dimension::Fixed(height));
    ftxui::Render(screen,element);
    Grid result{width,height,{}};
    result.cells.reserve(static_cast<std::size_t>(width * height));
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const auto& p = screen.PixelAt(x,y);
        result.cells.push_back({p.character,p.foreground_color.Print(false),p.background_color.Print(true)});
    }
    return result;
}
inline Grid scene(std::initializer_list<std::string_view> lines) {
    int width = 1;
    for (const auto s : lines) width = std::max(width,ftxui::string_width(std::string(s)));
    Grid result{width,static_cast<int>(lines.size()),{}};
    // Utf8ToGlyphs already returns the empty continuation cell for wide glyphs.
    for (const auto line : lines) {
        const auto glyphs = ftxui::Utf8ToGlyphs(std::string(line));
        for (const auto& g : glyphs) result.cells.push_back({g,{},{}});
        for (int n = static_cast<int>(glyphs.size()); n < width; ++n) result.cells.push_back({});
    }
    bounded(result);
    return result;
}
inline void write(Grid& g,Point p,std::string_view text) {
    for (const auto& s : ftxui::Utf8ToGlyphs(std::string(text))) { g.edit(p).text = s; ++p.x; }
}
inline bool numeric_token(std::string_view text) {
    std::size_t at = 0;
    if (at < text.size() && (text[at] == '+' || text[at] == '-')) ++at;
    bool digit = false;
    while (at < text.size() && text[at] >= '0' && text[at] <= '9') { digit = true; ++at; }
    if (at < text.size() && text[at] == '.') {
        ++at;
        while (at < text.size() && text[at] >= '0' && text[at] <= '9') { digit = true; ++at; }
    }
    if (!digit) return false;
    if (at < text.size() && (text[at] == 'e' || text[at] == 'E')) {
        ++at;
        if (at < text.size() && (text[at] == '+' || text[at] == '-')) ++at;
        const auto first = at;
        while (at < text.size() && text[at] >= '0' && text[at] <= '9') ++at;
        if (at == first) return false;
    }
    return at == text.size();
}
inline std::vector<Point> find_text(const Grid& g,std::string_view label,Work& work) {
    const auto glyphs = ftxui::Utf8ToGlyphs(std::string(label));
    if (glyphs.empty()) return {};
    if (glyphs.size() > 256) throw std::runtime_error("Chart label work bound exceeded");
    work.spend(label.size());
    const bool number_label = numeric_token(label);
    std::vector<Point> result;
    for (int y = 0; y < g.height; ++y) for (int x = 0; x + static_cast<int>(glyphs.size()) <= g.width; ++x) {
        work.spend();
        bool same = true;
        for (std::size_t n = 0; n < glyphs.size(); ++n) {
            work.spend();
            if (g.at({x + static_cast<int>(n),y}).text != glyphs[n]) { same = false; break; }
        }
        auto word = [](std::string_view s) {
            return s.size() == 1 && ((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= 'A' && s[0] <= 'Z') ||
                (s[0] >= '0' && s[0] <= '9') || s[0] == '_');
        };
        if (number_label) {
            auto number_part = [&](std::string_view s) { return word(s) || s == "." || s == "+" || s == "-"; };
            if (same && (number_part(g.at({x-1,y}).text) || number_part(g.at({x+static_cast<int>(glyphs.size()),y}).text))) same = false;
        } else {
            if (same && word(glyphs.front()) && word(g.at({x-1,y}).text)) same = false;
            if (same && word(glyphs.back()) && word(g.at({x+static_cast<int>(glyphs.size()),y}).text)) same = false;
        }
        if (same) result.push_back({x,y});
    }
    return result;
}
inline std::string trim(std::string s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    return s.substr(a,s.find_last_not_of(" \t\r\n") - a + 1);
}
struct Claims {
    const Grid& grid;
    std::vector<bool> cells;
    std::vector<std::uint32_t> data_owners;
    explicit Claims(const Grid& g) : grid(g),cells(g.cells.size()) {}
    void cell(Point p) { if (grid.has(p)) cells[static_cast<std::size_t>(p.y*grid.width+p.x)] = true; }
    void enable_data(Work& work) {
        work.spend(grid.cells.size());
        data_owners.assign(grid.cells.size(),0);
    }
    void data(Point p,std::size_t owner,Work& work) {
        cell(p);
        if (!data_owners.empty() && grid.has(p)) {
            work.spend(); data_owners[static_cast<std::size_t>(p.y*grid.width+p.x)] |= std::uint32_t{1} << owner;
        }
    }
    void text(Point p,std::string_view text) {
        const auto glyphs = ftxui::Utf8ToGlyphs(std::string(text));
        for (std::size_t n = 0; n < glyphs.size(); ++n) cell({p.x+static_cast<int>(n),p.y});
    }
    bool all_claimed(Work& work) const {
        const auto default_background = ftxui::Color(ftxui::Color::Default).Print(true);
        for (std::size_t n = 0; n < cells.size(); ++n) {
            work.spend();
            const auto& c = grid.cells[n];
            const bool ink = (!c.text.empty() && c.text != " ") ||
                (!c.background.empty() && c.background != default_background);
            if (ink && !cells[n]) return false;
        }
        return true;
    }
    void sentinel(Work& work,std::string_view following_token=kDefaultFollowingToken) {
        const auto matches = find_text(grid,following_token,work);
        if (matches.size() > 1) throw std::runtime_error("Duplicate chart sentinel");
        for (const auto p : matches) text(p,following_token);
    }
};
struct Number { double value = 0; int begin = 0,end = 0,precision = 0; bool percent = false; };
inline std::vector<Number> numbers(std::string_view s) {
    std::vector<Number> result;
    for (std::size_t i = 0; i < s.size();) {
        const auto start = i;
        if (s[i] == '-' || s[i] == '+') ++i;
        bool digit = false;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') { digit = true; ++i; }
        int precision = 0;
        if (i < s.size() && s[i] == '.') {
            ++i;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') { digit = true; ++i; ++precision; }
        }
        if (!digit) { i = start + 1; continue; }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            ++i;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
            const auto exponent = i;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
            if (exponent == i) { i = start + 1; continue; }
        }
        double value = 0;
        const auto text = s.substr(start,i-start);
        const auto parsed = std::from_chars(text.data(),text.data()+text.size(),value);
        if (parsed.ec != std::errc{} || !std::isfinite(value)) { i = start+1; continue; }
        const bool percent = i < s.size() && s[i] == '%';
        result.push_back({value,static_cast<int>(start),static_cast<int>(i),precision,percent});
        if (percent) ++i;
    }
    return result;
}
inline bool exact(double a,double b) {
    return std::isfinite(a) && std::isfinite(b) &&
        std::abs(a-b) <= 2 * std::numeric_limits<double>::epsilon() * std::max(1.0,std::abs(b));
}
inline bool has_value(std::string_view s,double value) {
    const auto ns = numbers(s);
    return std::ranges::any_of(ns,[&](const Number& n) { return !n.percent && exact(n.value,value); });
}
inline bool fill(std::string_view s) {
    constexpr std::array<std::string_view,17> fills{{"█","▓","▒","░","▉","▊","▋","▌","▍","▎","▏","■","#","*","+","=","▁"}};
    return std::ranges::find(fills,s) != fills.end();
}
inline bool same_swatch(const Cell& a,const Cell& b) {
    return a.text == b.text && a.foreground == b.foreground && a.background == b.background;
}
struct Slice { std::string label; double value = 0; };
struct Pie { std::string title; bool show_data = false; std::vector<Slice> slices; };
inline Pie pie_model(JsonVal value) {
    Pie p{value.get_string("title"),value.get("show_data").as_bool(),{}};
    value.get("slices").iter([&](JsonVal s) { p.slices.push_back({s.get_string("label"),s.get("value").as_double()}); });
    if (p.slices.size() > 256) throw std::runtime_error("Pie oracle slice bound exceeded");
    for (const auto& s : p.slices) if (s.label.empty() || !std::isfinite(s.value) || s.value <= 0)
        throw std::runtime_error("First chart batch requires named positive pie slices");
    return p;
}
inline constexpr std::string_view kPieSubcellLegend = "Scale: 40 cells = 100%; dot = positive share below 0.5 cell";
struct PieRow { int y = 0,separator = 0,start = 0,length = 0; Cell swatch; double percent = 0,tolerance = 0; bool raw_value = false; int label_x = 0; bool subcell = false; int suffix_start = 0; };
inline bool pie_subcell_annotation(std::string_view suffix,double value) {
    constexpr std::string_view tail = "; below 0.5 cell)";
    const auto open = suffix.find('('), end = suffix.find(tail);
    if (open == std::string_view::npos || end == std::string_view::npos || end <= open || suffix.find('(',open+1) != std::string_view::npos || suffix.find(tail,end+tail.size()) != std::string_view::npos) return false;
    const auto raw = suffix.substr(open+1,end-open-1);
    if (!numeric_token(raw)) return false;
    const auto ns = numbers(raw);
    return ns.size() == 1 && !ns[0].percent && exact(ns[0].value,value);
}
inline std::vector<std::string> pie_errors(const Grid& g,const Pie& model,Work& work,std::string_view following_token=kDefaultFollowingToken) {
    bounded(g);
    std::vector<std::string> errors;
    auto error = [&](std::string s) { errors.push_back(std::move(s)); };
    if (!model.title.empty() && find_text(g,model.title,work).empty()) error("Pie title missing");
    if (model.slices.empty()) {
        // Empty charts are explicitly classified separately, never as drawing.
        if (g.text().find('%') != std::string::npos || std::ranges::any_of(g.cells,[](const Cell& c) { return fill(c.text); }))
            error("Empty pie invented data marks/readouts");
        Claims claims(g); claims.sentinel(work,following_token);
        for (const auto p : find_text(g,model.title,work)) claims.text(p,model.title);
        constexpr std::array<std::string_view,3> empty_labels{{"No data","Empty chart","Empty pie chart"}};
        for (const auto label : empty_labels) for (const auto p : find_text(g,label,work)) claims.text(p,label);
        if (!claims.all_claimed(work)) error("Empty pie contains source or unclaimed data/text");
        return errors;
    }
    double total = 0;
    for (const auto& s : model.slices) total += s.value;
    if (!std::isfinite(total) || total <= 0) throw std::runtime_error("Invalid frozen pie total");
    std::vector<PieRow> rows;
    for (const auto& slice : model.slices) {
        std::vector<PieRow> candidates;
        for (const auto label : find_text(g,slice.label,work)) {
            const int end = label.x + ftxui::string_width(slice.label);
            int separator = end;
            while (separator < g.width && g.at({separator,label.y}).text == " ") { ++separator; work.spend(); }
            if (separator >= g.width || (g.at({separator,label.y}).text != "┃" && g.at({separator,label.y}).text != "│")) continue;
            int start = separator+1;
            while (start < g.width && g.at({start,label.y}).text == " ") { ++start; work.spend(); }
            const bool subcell = g.at({start,label.y}).text == "·";
            if (!subcell && !fill(g.at({start,label.y}).text)) continue;
            const Cell swatch = g.at({start,label.y});
            int stop = start;
            if (subcell) ++stop;
            else while (stop < g.width && same_swatch(g.at({stop,label.y}),swatch)) { ++stop; work.spend(); }
            std::string suffix;
            for (int x = stop; x < g.width; ++x) { suffix += g.at({x,label.y}).text; work.spend(); }
            bool another_fill = false;
            for (int x = stop; x < g.width; ++x) if (fill(g.at({x,label.y}).text)) another_fill = true;
            if (another_fill) continue; // A gap or a foreign slice cannot be hidden in the bar.
            const auto ns = numbers(suffix);
            std::vector<Number> percentages;
            for (const auto& n : ns) if (n.percent) percentages.push_back(n);
            if (percentages.size() != 1) continue;
            const auto p = percentages.front();
            const double tolerance = 0.5 * std::pow(10.0,-p.precision) + 1e-9;
            if (std::abs(p.value - 100 * slice.value / total) > tolerance) continue;
            if (subcell && (!(40 * slice.value / total < 0.5) || !pie_subcell_annotation(suffix,slice.value))) continue;
            candidates.push_back({label.y,separator,start,subcell ? 0 : stop-start,swatch,p.value,tolerance,has_value(suffix,slice.value),label.x,subcell,stop});
        }
        if (candidates.size() != 1) { error("Pie owner row missing/ambiguous: " + slice.label); continue; }
        rows.push_back(candidates.front());
    }
    if (rows.size() != model.slices.size()) return errors;
    for (std::size_t n = 0; n < rows.size(); ++n) {
        if (n && (rows[n].y <= rows[n-1].y || rows[n].separator != rows[0].separator || rows[n].start != rows[0].start))
            error("Pie slice order/common baseline changed");
        if (model.show_data && !rows[n].raw_value) error("showData raw value missing from owned row");
        for (std::size_t m = 0; m < n; ++m) if (!rows[n].subcell && !rows[m].subcell && same_swatch(rows[n].swatch,rows[m].swatch)) error("Pie slice swatches are not distinct");
    }
    const bool any_subcell = std::ranges::any_of(rows,[](const PieRow& r) { return r.subcell; });
    const auto legends = find_text(g,kPieSubcellLegend,work);
    if (legends.size() != (any_subcell ? 1u : 0u)) error("Pie below-resolution scale legend missing/duplicated/unexpected");
    if (any_subcell && legends.size() == 1 && std::ranges::any_of(rows,[&](const PieRow& r) { return legends[0].y == r.y; })) error("Pie scale legend must have its own row");
    double percent_sum = 0,tolerance_sum = 0;
    for (const auto& r : rows) { percent_sum += r.percent; tolerance_sum += r.tolerance; }
    if (std::abs(percent_sum-100) > tolerance_sum) error("Pie percentages changed the common total");
    // A source-order stacked overview, when present, fixes the 100% capacity.
    // It must contain ONLY owned swatches; an arbitrary extra glyph is invalid.
    std::vector<std::pair<int,int>> overviews;
    for (int y = 0; y < rows[0].y; ++y) {
        int first = -1,last = -1;
        for (int x = 0; x < g.width; ++x) {
            work.spend();
            if (std::ranges::any_of(rows,[&](const PieRow& r) { return same_swatch(g.at({x,y}),r.swatch); })) {
                if (first < 0) first = x;
                last = x;
            }
        }
        if (first >= 0 && last >= first) overviews.emplace_back(y,first);
    }
    bool scale_found = false; int chosen_capacity = 0;
    for (int capacity = any_subcell ? 40 : 1; capacity <= (any_subcell ? 40 : kMaxWidth); ++capacity) {
        bool valid = true;
        for (std::size_t n = 0; n < rows.size(); ++n) {
            work.spend();
            const int expected = static_cast<int>(std::floor(capacity * model.slices[n].value / total + 0.5));
            if (rows[n].length != expected) valid = false;
            for (std::size_t m = 0; m < n; ++m) if (!exact(model.slices[n].value,model.slices[m].value) &&
                rows[n].length == rows[m].length && (!rows[n].raw_value || !rows[m].raw_value)) valid = false;
        }
        if (any_subcell && !overviews.empty()) valid = false;
        for (const auto [y,start] : overviews) {
            int boundary = 0; double cumulative = 0;
            for (std::size_t n = 0; n < rows.size(); ++n) {
                cumulative += model.slices[n].value;
                const int end = static_cast<int>(std::floor(capacity * cumulative / total + 0.5));
                for (int x = boundary; x < end; ++x) { work.spend(); if (!same_swatch(g.at({start+x,y}),rows[n].swatch)) valid = false; }
                boundary = end;
            }
            if (fill(g.at({start+capacity,y}).text)) valid = false;
        }
        if (valid) { scale_found = true; chosen_capacity = capacity; break; }
    }
    if (!scale_found) error("Pie bars/overview do not share one round-half-up total scale");
    Claims claims(g); claims.sentinel(work,following_token);
    if (!model.title.empty()) {
        std::vector<Point> titles;
        for (const auto p : find_text(g,model.title,work)) if (p.y < rows.front().y) titles.push_back(p);
        if (titles.size() != 1) error("Pie title is not uniquely bound above data rows");
        else claims.text(titles.front(),model.title);
    }
    if (any_subcell && legends.size() == 1) claims.text(legends[0],kPieSubcellLegend);
    for (std::size_t n = 0; n < rows.size(); ++n) {
        const auto& r = rows[n]; claims.text({r.label_x,r.y},model.slices[n].label);
        claims.cell({r.separator,r.y});
        for (int x = r.start; x < r.start+r.length; ++x) claims.cell({x,r.y});
        if (r.subcell) claims.cell({r.start,r.y});
        const int suffix_start = r.subcell ? r.suffix_start : r.start+r.length;
        std::string suffix;
        for (int x = suffix_start; x < g.width; ++x) suffix += g.at({x,r.y}).text;
        if (r.subcell) {
            const auto open = suffix.find('('), end = suffix.find("; below 0.5 cell)");
            if (open != std::string::npos && end != std::string::npos) {
                const auto annotation = std::string_view(suffix).substr(open,end+std::string_view("; below 0.5 cell)").size()-open);
                claims.text({suffix_start+static_cast<int>(open),r.y},annotation);
            }
        }
        for (const auto& number : numbers(suffix)) {
            if (!number.percent && !exact(number.value,model.slices[n].value)) continue;
            for (int x = number.begin; x < number.end+(number.percent ? 1 : 0); ++x)
                claims.cell({suffix_start+x,r.y});
        }
        for (int x = suffix_start; x < g.width; ++x) {
            const auto t = g.at({x,r.y}).text;
            if (t == "(" || t == ")") claims.cell({x,r.y});
        }
    }
    if (scale_found) for (const auto [y,start] : overviews)
        for (int x = 0; x < chosen_capacity; ++x) claims.cell({start+x,y});
    if (!claims.all_claimed(work)) error("Pie contains unclaimed slice/data/text/plot marks");
    return errors;
}
inline std::vector<std::string> pie_errors(const Grid& g,const Pie& model) {
    Work work; return pie_errors(g,model,work,kDefaultFollowingToken);
}
// The Pie7 native prepay is retained as the existing formula, but the direct
// Screen::PixelAt acquisition now has one owner.
inline Grid capture_paid(ftxui::Element element,int width,int height,Work& work) {
    if (width <= 0 || width > kMaxWidth || height <= 0 || height > kMaxHeight)
        throw std::runtime_error("Chart capture bounds exceeded");
    const auto cells = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    work.spend(cells*(sizeof(ftxui::Pixel)+4u*sizeof(Cell)+256u)+8192u);
    return capture(element,width,height);
}
} // namespace loom::test::mermaid_chart
