module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;
import loom.serdes.json;

namespace loom::ui::mermaid::detail {
namespace {
constexpr std::array<std::string_view,16> kFills{{
    "█","░","▒","▓","▉","▊","▋","▌","▍","▎","▏","■","#","*","+","="}};
constexpr int kPlotHeight = 20,kHorizontalSpan = 40,kPieCapacity = 40;
// RFC 0031 E5: one parse boundary fee dominates the former per-line,
// per-event and per-scanner-token bookkeeping. Every deleted scan is a
// capped linear walk over the <=65536-byte source (the 2/byte term covers a
// two-pass walk); inputs that survive validation are bounded by the
// 256-glyph label limit, <=256 categories, <=32 series and <=16 pie slices,
// whose fixed storage the constant term dominates.
constexpr std::size_t kChartParsePerByte = 2u;
constexpr std::size_t kChartParseFixed = 32768u;
constexpr std::string_view kPieSubcellLegend =
    "Scale: 40 cells = 100%; dot = positive share below 0.5 cell";
std::string_view trim_chart(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}
std::optional<double> number(std::string_view s) {
    s = trim_chart(s); if (s.empty()) return {};
    double value = 0;
    const auto result = std::from_chars(s.data(),s.data()+s.size(),value);
    if (result.ec != std::errc{} || result.ptr != s.data()+s.size() || !std::isfinite(value)) return {};
    return value;
}
std::optional<double> xy_number(std::string_view s) {
    s = trim_chart(s);
    // Mermaid XY's signed decimal token differs from the Pie/YAML number
    // policies. In particular, signs cannot be separated from their digits.
    std::size_t at = 0;
    if (at < s.size() && (s[at] == '+' || s[at] == '-')) ++at;
    const auto start = at;
    while (at < s.size() && s[at] >= '0' && s[at] <= '9') ++at;
    const bool integral = at > start;
    if (at < s.size() && s[at] == '.') {
        const auto decimal = ++at;
        while (at < s.size() && s[at] >= '0' && s[at] <= '9') ++at;
        if (at == decimal) return {};
    } else if (!integral) return {};
    if (at != s.size()) return {};
    if (s.front() == '+') s.remove_prefix(1);
    return number(s);
}
std::string numeric(double value,bool odd_width = false) {
    std::array<char,64> buffer{};
    const auto result = std::to_chars(buffer.data(),buffer.data()+buffer.size(),value);
    if (result.ec != std::errc{}) return {};
    std::string s(buffer.data(),result.ptr);
    // Horizontal numeric tick labels must have an exact integer-cell center.
    // Appending insignificant decimal zeros changes no numeric information.
    if (odd_width && s.size()%2 == 0) {
        const auto exponent = s.find_first_of("eE");
        const auto at = exponent == std::string::npos ? s.size() : exponent;
        const auto dot = s.find('.');
        s.insert(at,dot != std::string::npos && dot < at ? "0" : ".00");
    }
    return s;
}
std::optional<std::string> chart_label(std::string_view s,bool quoted = false) {
    if (!quoted) s = trim_chart(s);
    // Presentation syntax must be understood before plain-text validation;
    // silently printing HTML, entities or Markdown would change its meaning.
    if (s.find_first_of("<>`\\\"") != std::string_view::npos) return {};
    for (const char marker : {'&','#'}) {
        const auto at = s.find(marker);
        if (at != std::string_view::npos && s.find(';',at) != std::string_view::npos) return {};
    }
    return validated_plain_label(s);
}
bool take_word(std::string_view& s,std::string_view word) {
    s = trim_chart(s);
    if (!s.starts_with(word) || (s.size() > word.size() && s[word.size()] != ' ' && s[word.size()] != '\t' && s[word.size()] != '[')) return false;
    s.remove_prefix(word.size()); s = trim_chart(s); return true;
}
std::optional<std::string> take_quoted(std::string_view& s) {
    s = trim_chart(s);
    if (s.empty() || s.front() != '"') return {};
    const auto end = s.find('"',1);
    if (end == std::string_view::npos) return {};
    const auto value = chart_label(s.substr(1,end-1),true);
    s.remove_prefix(end+1); s = trim_chart(s); return value;
}
std::optional<std::vector<std::string_view>> list(std::string_view s) {
    s = trim_chart(s);
    if (s.size() < 2 || s.front() != '[' || s.back() != ']') return {};
    s = s.substr(1,s.size()-2);
    std::vector<std::string_view> result;
    bool quoted = false; std::size_t start = 0;
    for (std::size_t at = 0; at <= s.size(); ++at) {
        if (at < s.size() && s[at] == '"') quoted = !quoted;
        if (at < s.size() && (s[at] != ',' || quoted)) continue;
        const auto value = trim_chart(s.substr(start,at-start));
        if (value.empty()) return {};
        result.push_back(value); start = at+1;
    }
    if (quoted || result.empty()) return {};
    return result;
}
struct ChartSeries { bool bar = true; std::string name; std::vector<double> values; bool named = false; };
struct ChartSlice { std::string label; double value = 0; };
struct Chart {
    bool pie = false,horizontal = false,show_data = false,show_data_outside = false,configured = false;
    std::string title,x_title,y_title;
    std::vector<std::string> categories;
    std::optional<std::pair<double,double>> domain;
    std::optional<std::pair<double,double>> x_domain;
    std::vector<ChartSeries> series;
    std::vector<ChartSlice> slices;
};
// RFC 0031 E1: frontmatter structure is owned by the shared envelope reader;
// this keeps only the reviewed XY terminal-profile value rules.
std::optional<std::string_view> chart_yaml_scalar(std::string_view value) {
    if (value.empty()) return {};
    if (value.front() == '\'' || value.front() == '"') {
        if (value.size() < 2 || value.back() != value.front()) return {};
        const char quote = value.front(); value = value.substr(1,value.size()-2);
        if (value.find(quote) != std::string_view::npos || value.find('\\') != std::string_view::npos) return {};
    } else if (value.find_first_of("{}[]&*!|>#'\"\\") != std::string_view::npos) return {};
    return value;
}
bool chart_color_value(std::string_view value) {
    if ((value.size() != 4 && value.size() != 7 && value.size() != 9) || value.front() != '#') return false;
    return std::ranges::all_of(value.substr(1),[](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}
bool xy_visual_envelope(const Envelope& envelope,Chart& chart) {
    constexpr std::array<std::string_view,5> kMaps{{"xyChart","xyChart.xAxis","xyChart.yAxis",
        "themeVariables","themeVariables.xyChart"}};
    std::set<std::string> maps,maps_with_children;
    std::map<std::string,int> child_indent;
    bool config_seen = false;
    for (const EnvelopeEvent& event : envelope.events) {
        if (event.blank) continue; // the old line splitter dropped blank lines
        if (event.comment) return false;
        if (event.top_level) {
            if (event.path != "config" || event.indent != 0 || config_seen) return false;
            config_seen = true;
            continue;
        }
        if (event.indent == 0) return false;
        const auto parent_dot = event.path.rfind('.');
        const auto parent = parent_dot == std::string::npos ? std::string{} : event.path.substr(0,parent_dot);
        const auto [indent_position,indent_inserted] = child_indent.emplace(parent,event.indent);
        if (!indent_inserted && indent_position->second != event.indent) return false;
        if (event.namespace_entry) {
            if (std::ranges::find(kMaps,event.path) == kMaps.end()) return false;
            if (!maps.emplace(event.path).second) return false;
            continue;
        }
        for (auto parent = event.path; true;) {
            const auto dot = parent.rfind('.');
            if (dot == std::string::npos) break;
            parent = parent.substr(0,dot);
            maps_with_children.insert(parent);
        }
        const auto raw = event.value;
        const auto value = chart_yaml_scalar(raw);
        if (!value) return false;
        if (event.path == "theme") {
            if (*value != "base" && *value != "dark" && *value != "default" && *value != "forest" && *value != "neutral") return false;
            continue;
        }
        if (event.path.starts_with("themeVariables.xyChart.")) {
            const auto role = std::string_view(event.path).substr(std::string_view("themeVariables.xyChart.").size());
            if (role == "plotColorPalette") {
                auto remaining = *value;
                do {
                    const auto comma = remaining.find(',');
                    if (!chart_color_value(trim_chart(remaining.substr(0,comma)))) return false;
                    if (comma == std::string_view::npos) break;
                    remaining.remove_prefix(comma+1);
                } while (true);
            } else {
                constexpr std::array<std::string_view,10> roles{{"backgroundColor","titleColor","xAxisLabelColor","xAxisLineColor",
                    "xAxisTickColor","xAxisTitleColor","yAxisLabelColor","yAxisLineColor","yAxisTickColor","yAxisTitleColor"}};
                if (std::ranges::find(roles,role) == roles.end() || !chart_color_value(*value)) return false;
            }
            continue;
        }
        if (!event.path.starts_with("xyChart.")) return false;
        auto field = std::string_view(event.path).substr(std::string_view("xyChart.").size());
        if (field == "chartOrientation") {
            if (*value == "horizontal") chart.horizontal = true;
            else if (*value == "vertical") chart.horizontal = false;
            else return false;
            continue;
        }
        if (raw.front() == '\'' || raw.front() == '"') return false;
        if (field == "showDataLabel" || field == "showDataLabelOutsideBar") {
            if (*value != "true" && *value != "false") return false;
            (field == "showDataLabel" ? chart.show_data : chart.show_data_outside) = *value == "true";
            continue;
        }
        if (field == "showTitle" || field == "showLegend") { if (*value != "true") return false; continue; }
        const bool axis = field.starts_with("xAxis.") || field.starts_with("yAxis.");
        if (axis) field.remove_prefix(6);
        if (axis && (field == "showTitle" || field == "showLabel" || field == "showTick" || field == "showAxisLine")) {
            if (*value != "true") return false;
            continue;
        }
        constexpr std::array<std::string_view,7> axis_metrics{{"axisLineWidth","labelFontSize","labelPadding","tickLength","tickWidth","titleFontSize","titlePadding"}};
        constexpr std::array<std::string_view,5> chart_metrics{{"width","height","titleFontSize","titlePadding","plotReservedSpacePercent"}};
        if (axis ? std::ranges::find(axis_metrics,field) == axis_metrics.end() : std::ranges::find(chart_metrics,field) == chart_metrics.end()) return false;
        const auto metric = number(*value);
        if (!metric || *metric < 0 || (field == "plotReservedSpacePercent" && *metric > 100)) return false;
    }
    if (!config_seen || envelope.config.empty()) return false;
    for (const auto& map : maps) if (!maps_with_children.contains(map)) return false;
    return true;
}
std::optional<std::string> xy_text(std::string_view text,bool quoted = false) {
    if (quoted) return chart_label(trim_chart(text));
    std::string joined;
    for (const unsigned char c : text) {
        if (c == ' ' || c == '\t') continue;
        // Keep the pinned terminal parser's Unicode labels while matching
        // Mermaid alphaNumToken admission for all ASCII source characters.
        if (c < 128 && !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || std::string_view("+*=.#-&_").find(static_cast<char>(c)) != std::string_view::npos)) return {};
        joined += static_cast<char>(c);
    }
    return chart_label(joined);
}
std::optional<std::string> xy_series_text(std::string_view text,bool unicode = false) {
    // In the pinned grammar a reserved lexer token cannot be alphaNum text.
    // Longest matching ASCII letter runs keep names such as lineTitle1 valid.
    // RFC 0031 E5: the per-char walk is input-bounded scanner work; the parse
    // entry fee dominates it.
    constexpr std::array<std::string_view,9> reserved{{"title","line","bar","xychart-beta","xychart",
        "x-axis","y-axis","vertical","horizontal"}};
    auto alpha = [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
    auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c-'A'+'a') : c; };
    for (std::size_t at = 0; at < text.size();) {
        const auto c = static_cast<unsigned char>(text[at]);
        if (c >= 128) {
            if (!unicode) return {}; // Existing series names still require quotes.
            ++at; continue; // Empty titles retain the pinned terminal Unicode form.
        }
        if (!alpha(c)) { ++at; continue; }
        std::size_t end = at+1;
        while (end < text.size() && alpha(static_cast<unsigned char>(text[end]))) ++end;
        for (const auto keyword : reserved) {
            if (keyword.size() < end-at || keyword.size() > text.size()-at) continue;
            bool matches = true;
            for (std::size_t n = 0; n < keyword.size(); ++n)
                if (lower(static_cast<unsigned char>(text[at+n])) != static_cast<unsigned char>(keyword[n])) { matches = false; break; }
            if (matches) return {};
        }
        at = end;
    }
    return xy_text(text);
}
bool xy_axis(std::string_view rest,bool x,std::string& title,std::vector<std::string>& categories,
             std::optional<std::pair<double,double>>& domain) {
    bool titled = false;
    auto set_title = [&](std::string_view text,bool quoted) {
        if (titled) return false;
        const auto label = xy_text(text,quoted);
        if (!label) return false;
        title = *label; titled = true; return true;
    };
    if (!rest.empty() && rest.front() == '"') {
        const auto end = rest.find('"',1);
        if (end == std::string_view::npos || !set_title(rest.substr(1,end-1),true)) return false;
        rest = trim_chart(rest.substr(end+1));
    }
    const auto bracket = rest.find('[');
    if (bracket != std::string_view::npos) {
        if (!x || (bracket && !set_title(rest.substr(0,bracket),false))) return false;
        const auto values = list(rest.substr(bracket)); if (!values) return false;
        for (auto token : *values) {
            std::optional<std::string> label;
            if (token.front() == '"') {
                label = take_quoted(token); if (!label || !token.empty()) return false;
                label = xy_text(*label,true);
            }
            else label = xy_text(token);
            if (!label) return false;
            categories.push_back(std::move(*label));
        }
        return true;
    }
    const auto arrow = rest.find("-->");
    if (arrow == std::string_view::npos) return rest.empty() ? titled : set_title(rest,false);
    auto lower = trim_chart(rest.substr(0,arrow));
    auto lo = xy_number(lower);
    if (!lo) {
        const auto space = lower.find_last_of(" \t");
        if (space == std::string_view::npos || !set_title(lower.substr(0,space),false)) return false;
        lo = xy_number(lower.substr(space+1));
    }
    const auto hi = xy_number(rest.substr(arrow+3));
    if (!lo || !hi || *lo == *hi || !std::isfinite(*hi-*lo)) return false;
    domain = {{*lo,*hi}}; return true;
}
// RFC 0031 E1: the shared envelope extracted the directive span and JSON
// text; the reviewed xyChart boolean profile is still family validation.
bool xy_init_json(std::string_view json,Chart& chart) {
    const auto document = loom::utils::json::parse(json);
    if (!document || !document->root().is_obj() || document->root().size() != 1) return false;
    const auto options = document->root().get("xyChart");
    if (!options.is_obj() || options.size() == 0 || options.size() > 2) return false;
    bool valid = true,label = chart.show_data,outside = chart.show_data_outside;
    std::set<std::string_view> seen;
    options.iter_obj([&](loom::utils::json::JsonVal key,loom::utils::json::JsonVal value) {
        const auto name = key.as_str();
        if (!seen.insert(name).second || !value.is_bool()) { valid = false; return; }
        if (name == "showDataLabel") label = value.as_bool();
        else if (name == "showDataLabelOutsideBar") outside = value.as_bool();
        else valid = false;
    });
    if (!valid) return false;
    chart.show_data = label; chart.show_data_outside = outside; return true;
}
std::optional<std::string> pie_title(std::string_view text) {
    // The primary TITLE converter trims and collapses horizontal whitespace.
    // RFC 0031 E5: the normalized copy is a validated <=256-glyph label; the
    // parse entry fee dominates its storage.
    if (text.size() > kSourceLimit) return {};
    text = trim_chart(text);
    if (text.empty()) return std::string{};
    if (text.front() == '"') {
        auto quoted = take_quoted(text);
        if (!quoted || !text.empty()) return {};
        return quoted;
    }
    std::string normalized;
    normalized.reserve(text.size());
    bool pending_space = false;
    for (const char c : text) {
        if (c == ' ' || c == '\t') { pending_space = !normalized.empty(); continue; }
        if (pending_space) { normalized.push_back(' '); pending_space = false; }
        normalized.push_back(c);
    }
    return chart_label(normalized,true);
}
// RFC 0031 E1: the reviewed Pie folded-description profile stays stricter
// than the shared structural reader — literal `description: >-` header and
// every payload line indented by exactly two physical spaces.
bool pie_description_frontmatter(const Envelope& envelope) {
    const auto& description = envelope.description;
    if (!description || description->form != EnvelopeScalarForm::Folded ||
        description->header != ">-") return false;
    bool first = true,have_payload = false;
    for (auto rest = envelope.raw_frontmatter; !rest.empty();) {
        const auto newline = rest.find('\n');
        auto raw = rest.substr(0,newline == std::string_view::npos ? rest.size() : newline);
        if (raw.ends_with('\r')) raw.remove_suffix(1);
        if (first) {
            if (raw != "description: >-") return false;
            first = false;
        } else if (!raw.empty()) {
            if (raw.size() <= 2u || raw[0] != ' ' || raw[1] != ' ' || raw[2] == ' ' || raw[2] == '\t') return false;
            have_payload = true;
        }
        if (newline == std::string_view::npos) break;
        rest = rest.substr(newline+1);
    }
    return have_payload && !first;
}
// The narrow Pie config profile admits exactly config.pie.textPosition with
// a plain numeric scalar in the reviewed 0..1 range; nothing else.
bool pie_config_frontmatter(const Envelope& envelope) {
    bool config_seen = false,pie_seen = false,position_seen = false;
    for (const EnvelopeEvent& event : envelope.events) {
        if (event.blank) continue; // the old line splitter dropped blank lines
        if (event.comment) return false;
        if (event.top_level) {
            if (event.path != "config" || event.indent != 0 || config_seen) return false;
            config_seen = true;
            continue;
        }
        if (event.namespace_entry) {
            if (event.path != "pie" || event.indent == 0 || pie_seen) return false;
            pie_seen = true;
            continue;
        }
        if (event.path != "pie.textPosition" || position_seen) return false;
        const auto raw = event.value;
        const auto value = chart_yaml_scalar(raw);
        if (!value || raw.front() == '\'' || raw.front() == '"') return false;
        const auto position = number(*value);
        if (!position || *position < 0 || *position > 1) return false;
        position_seen = true;
    }
    return config_seen && pie_seen && position_seen;
}
std::optional<std::vector<std::string_view>> chart_source_lines(std::string_view source,Chart& chart,Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    // Parse boundary fee (RFC 0031 E5), charged before the envelope read and
    // every source/envelope scan below.
    if (source.size() > (std::numeric_limits<std::size_t>::max() - kChartParseFixed) / kChartParsePerByte ||
        !work.spend(kChartParseFixed + kChartParsePerByte * source.size())) return {};
    const auto envelope = detail::parse_envelope(source,work);
    if (!envelope) return {};
    enum class Profile { Plain, PieDescription, PieConfig, Xy } profile = Profile::Plain;
    if (envelope->has_frontmatter) {
        if (!envelope->init.empty()) return {};
        const bool pie_config = std::ranges::any_of(envelope->config,
            [](const EnvelopeEvent& event){ return event.path == "pie.textPosition"; });
        if (envelope->description) {
            if (pie_config || !pie_description_frontmatter(*envelope)) return {};
            profile = Profile::PieDescription;
        } else if (pie_config) {
            if (!pie_config_frontmatter(*envelope)) return {};
            profile = Profile::PieConfig;
        } else {
            if (!xy_visual_envelope(*envelope,chart)) return {};
            profile = Profile::Xy;
        }
        chart.configured = true;
    } else if (!envelope->init.empty()) {
        if (envelope->init.size() != 1u) return {};
        if (!xy_init_json(envelope->init.front().json,chart)) return {};
        chart.configured = true;
        profile = Profile::Xy;
    }
    // A directive is admitted only when it leads the source; any directive at
    // or after the first body statement preserves the source.
    if (!envelope->init.empty()) {
        if (envelope->effective_body.empty()) return {};
        const auto effective_at = static_cast<std::size_t>(envelope->effective_body.data()-source.data());
        for (const auto& init : envelope->init) if (init.begin >= effective_at) return {};
    }
    const auto body = envelope->init.empty() ? envelope->raw_body : envelope->effective_body;
    std::vector<std::string_view> lines;
    for (auto rest = body; !rest.empty();) {
        const auto newline = rest.find('\n');
        auto raw = rest.substr(0,newline == std::string_view::npos ? rest.size() : newline);
        if (raw.ends_with('\r')) raw.remove_suffix(1);
        const auto line = trim_chart(raw);
        if (!line.empty() && !line.starts_with("%%")) lines.push_back(line);
        if (newline == std::string_view::npos) break;
        rest = rest.substr(newline+1);
    }
    if (lines.empty()) return {};
    if (profile == Profile::PieDescription || profile == Profile::PieConfig) {
        if (!lines.front().starts_with("pie")) return {};
    } else if (profile == Profile::Xy) {
        if (!lines.front().starts_with("xychart")) return {};
    }
    return lines;
}
std::optional<Chart> parse_chart(std::string_view source,Work& work) {
    Chart chart;
    const auto lines = chart_source_lines(source,chart,work); if (!lines) return {};
    auto header = lines->front();
    bool title_seen = false,repeated_title = false,x_seen = false,y_seen = false;
    if (take_word(header,"pie")) {
        chart.pie = true;
        // This new physical-line check belongs only to the approved Pie
        // extension. Existing XY source cost and behavior remain unchanged.
        // RFC 0031 E5: the linear CR scan is covered by the parse entry fee.
        for (std::size_t at = 0; at < source.size(); ++at)
            if (source[at] == '\r' && (at+1u == source.size() || source[at+1u] != '\n')) return {};
        if (take_word(header,"showData")) chart.show_data = true;
        if (!header.empty()) {
            if (!take_word(header,"title")) return {};
            auto title = pie_title(header); if (!title) return {};
            chart.title = std::move(*title); title_seen = true; header = {};
        }
    } else if (take_word(header,"xychart-beta") || take_word(header,"xychart")) {
        if (take_word(header,"horizontal")) chart.horizontal = true;
        else if (take_word(header,"vertical")) chart.horizontal = false;
    } else return {};
    if (!header.empty()) return {};
    std::string_view xy_title;
    int bars = 0,line_series = 0;
    for (std::size_t n = 1; n < lines->size(); ++n) {
        auto line = (*lines)[n];
        auto rest = line;
        if (take_word(rest,"title")) {
            if (!chart.pie) {
                // Inventory first: empty XY and plotted XY have different
                // source title profiles. The view remains bound to raw source.
                repeated_title |= title_seen; title_seen = true; xy_title = rest;
                continue;
            }
            if (title_seen) return {};
            title_seen = true;
            auto title = pie_title(rest);
            if (!title) return {};
            chart.title = std::move(*title); continue;
        }
        if (chart.pie) {
            auto label = take_quoted(rest);
            if (!label || rest.empty() || rest.front() != ':') return {};
            rest.remove_prefix(1); const auto value = number(rest);
            if (!value || *value <= 0) return {};
            chart.slices.push_back({std::move(*label),*value}); continue;
        }
        rest = line;
        if (take_word(rest,"x-axis")) {
            if (x_seen || !xy_axis(rest,true,chart.x_title,chart.categories,chart.x_domain)) return {};
            x_seen = true; continue;
        }
        rest = line;
        if (take_word(rest,"y-axis")) {
            std::vector<std::string> unused;
            if (y_seen || !xy_axis(rest,false,chart.y_title,unused,chart.domain)) return {};
            y_seen = true; continue;
        }
        rest = line; bool bar = true;
        if (take_word(rest,"bar")) bar = true;
        else if (take_word(rest,"line")) bar = false;
        else return {};
        ChartSeries series{bar,(bar ? "Bar " : "Line ") + std::to_string(bar ? ++bars : ++line_series),{}};
        if (!rest.empty() && rest.front() == '"') {
            const auto name = take_quoted(rest); if (!name) return {};
            const auto resolved = chart_label(trim_chart(*name)); if (!resolved) return {};
            series.name = *resolved; series.named = true;
        } else if (!rest.empty() && rest.front() != '[') {
            const auto bracket = rest.find('['); if (bracket == std::string_view::npos) return {};
            const auto resolved = xy_series_text(rest.substr(0,bracket)); if (!resolved) return {};
            series.name = *resolved; series.named = true; rest = trim_chart(rest.substr(bracket));
        }
        const auto values = list(rest); if (!values) return {};
        for (const auto value : *values) { const auto parsed = xy_number(value); if (!parsed) return {}; series.values.push_back(*parsed); }
        chart.series.push_back(std::move(series));
    }
    if (!chart.pie) {
        // No-data projection is selected only after every statement and all
        // metadata have been parsed. A zero-valued plot is still a plot.
        if (chart.series.empty()) {
            if (chart.configured || chart.show_data || chart.show_data_outside || chart.x_domain ||
                (chart.domain && chart.domain->first > chart.domain->second) ||
                !chart.x_title.empty() || chart.categories.size() > 256) return {};
            // Every setter must be valid even when a later one overwrites it.
            // Rescan only this empty profile, preserving nonempty/Pie Work.
            for (std::size_t n = 1; n < lines->size(); ++n) {
                auto rest = (*lines)[n];
                if (take_word(rest,"title")) {
                    const bool quoted = !rest.empty() && rest.front() == '"';
                    auto title = quoted ? take_quoted(rest) : xy_series_text(rest,true);
                    if (quoted && !rest.empty()) return {};
                    if (quoted && title) title = xy_text(*title,true);
                    if (!title) return {};
                    chart.title = std::move(*title); continue;
                }
                rest = (*lines)[n];
                if (take_word(rest,"x-axis")) {
                    // Empty X titles/numeric domains are already deferred. A
                    // band text token must not enter a reserved lexer state.
                    const auto values = list(rest); if (!values) return {};
                    for (const auto token : *values)
                        if (token.front() != '"' && !xy_series_text(token,true)) return {};
                    continue;
                }
                rest = (*lines)[n];
                if (take_word(rest,"y-axis") && !chart.y_title.empty() && rest.front() != '"') {
                    auto title = rest;
                    if (const auto arrow = rest.find("-->"); arrow != std::string_view::npos) {
                        const auto lower = trim_chart(rest.substr(0,arrow));
                        const auto space = lower.find_last_of(" \t");
                        if (space == std::string_view::npos) return {};
                        title = trim_chart(lower.substr(0,space));
                    }
                    // In axis_data a digit is NUMBER_WITH_DECIMAL, not the
                    // NUM token accepted by alphaNum outside that lexer state.
                    if (title.find_first_of("0123456789") != std::string_view::npos ||
                        !xy_series_text(title,true)) return {};
                }
            }
            return chart;
        }
        if (repeated_title) return {}; // Preserve the existing nonempty profile.
        if (title_seen) {
            auto rest = xy_title;
            auto title = !rest.empty() && rest.front() == '"' ? take_quoted(rest) : chart_label(rest);
            if (!title || (xy_title.find('"') != std::string_view::npos && !rest.empty())) return {};
            chart.title = std::move(*title);
        }
        if (chart.series.size() == 1 && chart.series.front().named &&
            (chart.show_data || chart.series.front().name.find(" + ") != std::string::npos)) return {};
        // The pinned band Db ignores surplus points after consuming the full
        // valid dataset. Never apply this to inferred or numeric x coordinates.
        if (!chart.categories.empty()) for (auto& series : chart.series) {
            if (series.values.size() <= chart.categories.size()) continue;
            series.values.resize(chart.categories.size());
        }
        if (chart.categories.empty() && !chart.x_domain && !chart.series.empty()) {
            const auto count = chart.series.front().values.size();
            if (count == 1) chart.categories.push_back("1");
            else if (count > 1) chart.x_domain = {{1,static_cast<double>(count)}};
        }
        const bool reversed_y = chart.domain && chart.domain->first > chart.domain->second;
        const bool reversed_x = chart.x_domain && chart.x_domain->first > chart.x_domain->second;
        if (reversed_y || reversed_x) {
            // Directed axes retain their authored order. This finite vertical
            // profile has one unnamed dataset and an explicit reversed Y span.
            if (!reversed_y || chart.horizontal || chart.configured || chart.show_data ||
                chart.show_data_outside || chart.series.size() != 1 || chart.series.front().named ||
                chart.series.front().values.empty() || chart.series.front().values.size() > 2) return {};
            const auto& series = chart.series.front();
            if (series.bar ? (chart.x_domain || (chart.categories.size() != 1 || chart.categories.front() != "1") || series.values.size() != 1)
                           : !reversed_x) return {};
            for (const auto value : series.values)
                if (value > chart.domain->first) return {};
        }
        if (chart.x_domain) {
            // The terminal projection places uniformly sampled numeric x
            // coordinates on equal cell intervals. Showing every sample's
            // numeric tick is this native profile's choice, not an upstream
            // requirement on tick density. Keep the authored numeric range;
            // never reparse a generated categorical Mermaid source.
            if (chart.horizontal || chart.series.empty() || (chart.series.front().values.size() < 2 && !reversed_x)) return {};
            const std::size_t count = chart.series.front().values.size();
            const auto [lo,hi] = *chart.x_domain;
            const double step = count == 1 ? hi-lo : (hi-lo)/static_cast<double>(count-1);
            if (!std::isfinite(step) || step == 0 || ((step < 0) != reversed_x)) return {};
            double previous = lo;
            for (std::size_t n = 0; n < count; ++n) {
                const double value = count == 1 ? lo : n+1 == count ? hi : lo+step*static_cast<double>(n);
                if (!std::isfinite(value) || value < std::min(lo,hi) || value > std::max(lo,hi) ||
                    (n && (reversed_x ? value >= previous : value <= previous))) return {};
                previous = value;
                auto label = numeric(value);
                if (label.empty()) return {};
                chart.categories.push_back(std::move(label));
            }
        }
        if (chart.categories.empty() || chart.series.empty()) return {};
        for (const auto& s : chart.series) if (s.values.size() != chart.categories.size()) return {};
    }
    return chart;
}
std::string percentage(double value,double total) {
    std::array<char,64> buffer{};
    const auto result = std::to_chars(buffer.data(),buffer.data()+buffer.size(),100*(value/total),std::chars_format::fixed,1);
    if (result.ec != std::errc{}) return {};
    return std::string(buffer.data(),result.ptr) + "%";
}
std::vector<std::string> chart_lines(Canvas& canvas) {
    int first = canvas.width;
    for (int y = 0; y < canvas.height; ++y) for (int x = 0; x < canvas.width; ++x) {
        if (!canvas.work.spend()) return {};
        if (canvas.at({x,y}).glyph != " ") first = std::min(first,x);
    }
    auto result = canvas.lines();
    if (first > 0 && first < canvas.width) for (auto& line : result) line.insert(0,static_cast<std::size_t>(first),' ');
    return result;
}
std::optional<std::vector<std::string>> pie_chart(const Chart& chart,Work& work) {
    if (chart.slices.empty()) return std::vector<std::string>{chart.title};
    // A monochrome terminal needs a distinct visible swatch for every slice.
    // Exhausting this palette preserves the complete source, never reused keys.
    if (chart.slices.size() > kFills.size()) return {};
    struct PieRenderRow { int extent = 0; bool subcell = false; std::string suffix; };
    // Prepay all new extent scans (including the slices^2 duplicate scan,
    // RFC 0031 E5), suffix strings, row-vector storage, title glyph
    // inspection and canvas width planning before their first allocation.
    constexpr std::size_t kPieDuplicatePerPair = 8u;
    if (chart.slices.size() > std::numeric_limits<std::size_t>::max() / chart.slices.size() /
            kPieDuplicatePerPair) return {};
    if (!work.spend(chart.slices.size()*(sizeof(PieRenderRow)+4u*sizeof(std::string)+512u)+
                    chart.slices.size()*chart.slices.size()*kPieDuplicatePerPair+
                    (chart.title.size()+1u)*(8u*sizeof(std::string)+32u)+4096u)) return {};
    double total = 0; int label = 0;
    for (const auto& slice : chart.slices) {
        total += slice.value;
        label = std::max(label,label_width(slice.label));
    }
    if (!std::isfinite(total) || total <= 0) return {};
    std::vector<PieRenderRow> rows;
    rows.reserve(chart.slices.size());
    bool has_subcell = false;
    int width = std::max(label+kPieCapacity+40,label_width(chart.title));
    for (std::size_t n = 0; n < chart.slices.size(); ++n) {
        const auto& slice = chart.slices[n];
        const double ideal = kPieCapacity*(slice.value/total);
        if (!std::isfinite(ideal) || ideal <= 0) return {};
        const int extent = static_cast<int>(std::floor(ideal+0.5));
        const bool subcell = extent == 0;
        if (subcell && !(ideal < 0.5)) return {};
        bool disclosure = chart.show_data;
        for (std::size_t other = 0; other < chart.slices.size(); ++other) {
            const int other_extent = static_cast<int>(std::floor(kPieCapacity*(chart.slices[other].value/total)+0.5));
            if (!subcell && chart.slices[other].value != slice.value && other_extent == extent) disclosure = true;
        }
        const auto share = percentage(slice.value,total);
        if (share.empty()) return {};
        std::string suffix = "  "+share;
        if (subcell) {
            const auto raw = numeric(slice.value);
            if (raw.empty()) return {};
            suffix += " ("+raw+"; below 0.5 cell)";
            has_subcell = true;
        } else if (disclosure) {
            const auto raw = numeric(slice.value);
            if (raw.empty()) return {};
            suffix += " ("+raw+")";
        }
        const int visible = subcell ? 1 : extent;
        width = std::max(width,label+1+visible+label_width(suffix));
        rows.push_back({extent,subcell,std::move(suffix)});
    }
    if (has_subcell) width = std::max(width,label_width(kPieSubcellLegend));
    const int title_rows = chart.title.empty() ? 0 : 1;
    const int height = static_cast<int>(chart.slices.size())+title_rows+(has_subcell ? 1 : 0);
    if (width > kWidthLimit || height > kHeightLimit || !work.spend(static_cast<std::size_t>(width*height))) return {};
    Canvas canvas(width,height,work);
    if (title_rows && !canvas.text({0,0},chart.title)) return {};
    for (std::size_t n = 0; n < chart.slices.size(); ++n) {
        const auto& slice = chart.slices[n]; const auto& row = rows[n];
        const int y = static_cast<int>(n)+title_rows;
        if (!canvas.text({label-label_width(slice.label),y},slice.label) || !canvas.put({label,y},"┃")) return {};
        if (row.subcell) {
            if (!canvas.put({label+1,y},"·")) return {};
        } else {
            for (int x = 0; x < row.extent; ++x) if (!canvas.put({label+1+x,y},kFills[n])) return {};
        }
        const int visible = row.subcell ? 1 : row.extent;
        if (!canvas.text({label+1+visible,y},row.suffix)) return {};
    }
    if (has_subcell && !canvas.text({0,title_rows+static_cast<int>(chart.slices.size())},kPieSubcellLegend)) return {};
    auto result = chart_lines(canvas); if (result.empty()) return {}; return result;
}
std::string_view data_line_glyph(unsigned bits,int style) {
    if (style == 0) {
        switch (bits) {
            case kRight|kDown: return "╭"; case kDown|kLeft: return "╮";
            case kLeft|kUp: return "╯"; case kUp|kRight: return "╰";
            default: return line_glyph(bits);
        }
    }
    if (style == 1) {
        switch (bits) {
            case kRight: case kLeft: case kRight|kLeft: return "━";
            case kDown: case kUp: case kDown|kUp: return "┃";
            case kRight|kDown: return "┏"; case kDown|kLeft: return "┓";
            case kLeft|kUp: return "┛"; case kUp|kRight: return "┗";
            case kRight|kDown|kLeft: return "┳"; case kDown|kLeft|kUp: return "┫";
            case kLeft|kUp|kRight: return "┻"; case kUp|kRight|kDown: return "┣";
            default: return "╋";
        }
    }
    switch (bits) {
        case kRight: case kLeft: case kRight|kLeft: return "═";
        case kDown: case kUp: case kDown|kUp: return "║";
        case kRight|kDown: return "╔"; case kDown|kLeft: return "╗";
        case kLeft|kUp: return "╝"; case kUp|kRight: return "╚";
        case kRight|kDown|kLeft: return "╦"; case kDown|kLeft|kUp: return "╣";
        case kLeft|kUp|kRight: return "╩"; case kUp|kRight|kDown: return "╠";
        default: return "╬";
    }
}
std::optional<std::vector<std::string>> empty_xy_chart(const Chart& chart,Work& work) {
    // These are complete authored-field disclosures, not fabricated plot axes.
    if (!work.spend(64+chart.categories.size()*32)) return {};
    std::vector<std::string> rows;
    auto add = [&](std::string_view prefix,std::string_view value = {}) {
        // RFC 0031 E5: rows are <=256 validated category labels plus a fixed
        // preamble; their byte total is source-bounded and prepaid at parse.
        std::string row(prefix); row += value;
        if (label_width(row) > kWidthLimit) return false;
        rows.push_back(std::move(row)); return true;
    };
    if (!add("XY chart: no data") ||
        !add(chart.horizontal ? "Orientation: horizontal" : "Orientation: vertical") ||
        (!chart.title.empty() && !add("Title: ",chart.title)) || !add("X axis: band")) return {};
    if (chart.categories.empty()) { if (!add("Categories: none")) return {}; }
    else for (std::size_t n = 0; n < chart.categories.size(); ++n) {
        if (!work.spend(32)) return {};
        if (!add("Category "+std::to_string(n+1)+": ",chart.categories[n])) return {};
    }
    if (!add("Y axis: linear") || (!chart.y_title.empty() && !add("Y title: ",chart.y_title))) return {};
    if (chart.domain) {
        if (!work.spend(128)) return {};
        const auto lo = numeric(chart.domain->first),hi = numeric(chart.domain->second);
        if (lo.empty() || hi.empty() || !add("Range: ",lo+" --> "+hi)) return {};
    } else if (!add("Range: unspecified")) return {};
    if (rows.size() > kHeightLimit) return {};
    std::size_t width = 0;
    for (const auto& row : rows) width = std::max(width,static_cast<std::size_t>(label_width(row)));
    if (!work.spend(width*rows.size())) return {};
    return rows;
}
std::optional<std::vector<std::string>> xy_chart(const Chart& chart,Work& work) {
    if (chart.series.empty()) return empty_xy_chart(chart,work);
    int bars = 0,lines = 0;
    for (const auto& s : chart.series) s.bar ? ++bars : ++lines;
    if (bars > static_cast<int>(kFills.size()) || lines > 3 || chart.series.size() > 32) return {};
    const bool reversed_y = chart.domain && chart.domain->first > chart.domain->second;
    const bool reversed_numeric_x = reversed_y && chart.x_domain.has_value();
    const bool reverse_singleton_x = reversed_numeric_x && chart.categories.size() == 1;
    std::array<std::string,2> reverse_endpoints;
    if (reversed_numeric_x) {
        // numeric() uses a bounded 64-byte formatter. Pay both retained labels
        // and their placement before formatting, measurement, or allocation.
        if (!work.spend(2u*(64u+sizeof(std::string))+128u)) return {};
        reverse_endpoints = {numeric(chart.x_domain->first),numeric(chart.x_domain->second)};
        if (reverse_endpoints[0].empty() || reverse_endpoints[1].empty()) return {};
    }
    const bool named_singleton = chart.series.size() == 1 && chart.series.front().named;
    const bool compact_values = chart.show_data && chart.series.size() == 1 && chart.series.front().bar;
    if (chart.show_data && (lines > 2 || (lines && chart.categories.size() < 2))) return {};
    double minimum = 0,maximum = 0;
    int category_width = 0;
    for (const auto& label : chart.categories) category_width = std::max(category_width,label_width(label));
    if (reversed_numeric_x) for (const auto& endpoint : reverse_endpoints)
        category_width = std::max(category_width,label_width(endpoint));
    for (const auto& s : chart.series) for (const auto value : s.values) {
        if (!work.spend()) return {};
        minimum = std::min(minimum,value); maximum = std::max(maximum,value);
    }
    if (chart.domain) { minimum = chart.domain->first; maximum = chart.domain->second; }
    if (minimum == maximum) maximum = minimum+1;
    const double range = maximum-minimum;
    const double reference = reversed_y ? minimum : chart.domain && minimum > 0 ? minimum : 0;
    if (!std::isfinite(range) || range == 0 || (!reversed_y && range < 0) ||
        (bars && (reference < std::min(minimum,maximum) || reference > std::max(minimum,maximum)))) return {};
    // Negative first-boundary bar/value semantics need their separate source
    // profile; ordinary label text cannot justify a different bar reference.
    if (chart.show_data && bars && minimum < 0) return {};
    for (const auto& s : chart.series) for (const auto value : s.values)
        if (reversed_y ? value > minimum : (value < minimum || value > maximum)) return {};
    std::array<double,5> tick_values{}; std::array<std::string,5> tick_labels{};
    int numeric_width = 0;
    for (int n = 0; n <= 4; ++n) {
        tick_values[static_cast<std::size_t>(n)] = n == 4 ? maximum : minimum+range*(n/4.0);
        tick_labels[static_cast<std::size_t>(n)] = numeric(tick_values[static_cast<std::size_t>(n)],chart.horizontal);
        if (tick_labels[static_cast<std::size_t>(n)].empty()) return {};
        numeric_width = std::max(numeric_width,static_cast<int>(tick_labels[static_cast<std::size_t>(n)].size()));
    }
    int top = (chart.title.empty() ? 0 : 1) + (chart.horizontal ? (chart.x_title.empty() ? 0 : 1) : (chart.y_title.empty() ? 0 : 1));
    if (compact_values && chart.show_data_outside && !chart.horizontal) ++top;
    const int left = chart.horizontal ? category_width+numeric_width/2+2 : reversed_numeric_x ?
        std::max(numeric_width+1,category_width/2+1) : numeric_width+1;
    const int stride = chart.horizontal ? std::max(4,bars*2+2) : std::max({6,category_width+3,bars*2+3});
    std::vector<int> category;
    for (std::size_t n = 0; n < chart.categories.size(); ++n) {
        if (!work.spend()) return {};
        category.push_back((chart.horizontal ? top+2 : left+stride/2) + static_cast<int>(n)*stride);
    }
    const int reverse_span = reversed_numeric_x ? std::max(12,static_cast<int>(chart.categories.size()-1)*stride) : 0;
    if (reversed_numeric_x) for (std::size_t n = 0; n < category.size(); ++n) {
        if (!work.spend()) return {};
        const double first = chart.x_domain->first,last = chart.x_domain->second;
        const double value = category.size() == 1 ? first : n+1 == category.size() ? last :
            first+(last-first)*(static_cast<double>(n)/static_cast<double>(category.size()-1));
        category[n] = left+static_cast<int>(std::round(reverse_span*((value-first)/(last-first))));
    }
    const int bottom = chart.horizontal ? category.back()+std::max(2,bars) : top+kPlotHeight;
    const int right = chart.horizontal ? left+kHorizontalSpan : reversed_numeric_x ? left+reverse_span : category.back()+stride/2;
    const int axis_footer = bottom+2;
    int footer = axis_footer + (chart.horizontal ? (chart.y_title.empty() ? 0 : 1) : (chart.x_title.empty() ? 0 : 1));
    const int legend_start = footer+1;
    const int right_label_pad = std::max(numeric_width/2,reversed_numeric_x ? category_width/2+1 : 0);
    const int width = std::max({right+right_label_pad+2,label_width(chart.title),label_width(chart.x_title),label_width(chart.y_title),24});
    // Room is reserved before allocating or painting; later exact disclosures
    // and composite keys must also fit these existing public canvas limits.
    const int reserve_rows = chart.series.size() > 1 || named_singleton ? static_cast<int>(chart.series.size()) : 0;
    int height = legend_start+reserve_rows;
    if (width > kWidthLimit || height > kHeightLimit || !work.spend(static_cast<std::size_t>(width*height))) return {};
    auto project = [&](double value) {
        const double unit = (value-minimum)/range;
        const double fraction = reversed_y ? std::clamp(unit,0.0,1.0) : unit;
        return static_cast<int>(std::round(chart.horizontal ? left+kHorizontalSpan*fraction : top+kPlotHeight*(1-fraction)));
    };
    // Explicit ascending positive domains start bars at their lower endpoint.
    // A line-only chart never projects an unused, possibly excluded reference.
    const int baseline = bars ? project(reference) : 0;
    std::vector<std::vector<bool>> disclosures(chart.series.size(),std::vector<bool>(chart.categories.size()));
    std::map<int,std::set<double>> quantized;
    bool data_span = lines > 0,shared_axis = false;
    for (const auto& s : chart.series) for (const auto value : s.values) {
        if (!work.spend(32)) return {};
        if (s.bar && value != reference) data_span = true;
        // A distinct frame family keeps source-owned line/axis unions visible.
        if (bars && reference > 0 && !s.bar && project(value) == baseline) shared_axis = true;
        quantized[project(value)].insert(value);
    }
    if (bars && reference > 0 && !data_span) return {};
    if (shared_axis && (lines > 2 || chart.categories.size() < 2)) return {};
    for (std::size_t n = 0; n < chart.series.size(); ++n) for (std::size_t c = 0; c < chart.categories.size(); ++c) {
        const double value = chart.series[n].values[c];
        if (chart.show_data && !chart.series[n].bar && !shared_axis && project(value) == (chart.horizontal ? left : bottom)) return {};
        disclosures[n][c] = chart.show_data ? !compact_values : (quantized[project(value)].size() > 1 ||
            ((chart.series[n].bar || shared_axis) && value != reference && project(value) == baseline));
        // A named singleton must preserve exact values that collapse to one
        // cell through a separately reviewed disclosure profile.
        if (named_singleton && disclosures[n][c]) return {};
        if (disclosures[n][c]) ++height;
    }
    // At most one composite per combination of visible owners; compute the
    // exact combinations from data layers before reserving their legend rows.
    const std::size_t area = static_cast<std::size_t>(width*height);
    if (height > kHeightLimit || !work.spend(area*(static_cast<std::size_t>(lines)+2))) return {};
    std::vector<std::uint32_t> owners(area);
    std::vector<std::vector<unsigned>> line_bits(static_cast<std::size_t>(lines),std::vector<unsigned>(area));
    std::vector<int> bar_style(chart.series.size(),-1),line_style(chart.series.size(),-1);
    std::vector<bool> tiny(area);
    int bar_number = 0,line_number = 0;
    auto index = [&](Point p) { return static_cast<std::size_t>(p.y*width+p.x); };
    for (std::size_t s = 0; s < chart.series.size(); ++s) {
        if (chart.series[s].bar) bar_style[s] = bar_number++;
        else line_style[s] = line_number++;
    }
    for (std::size_t s = 0; s < chart.series.size(); ++s) {
        const auto bit = std::uint32_t{1} << s;
        if (chart.series[s].bar) {
            for (std::size_t c = 0; c < chart.categories.size(); ++c) {
                const double value = chart.series[s].values[c]; if (value == reference) continue;
                const int end = project(value),step = end >= baseline ? 1 : -1;
                const bool on_category_axis = chart.horizontal ? end == left : end == bottom;
                const bool reviewed_shared_tick = !chart.horizontal && !chart.domain && chart.series.size() == 1 &&
                    value < 0 && end-baseline > 1;
                if (on_category_axis && !reviewed_shared_tick) return {};

                const int offset = bars > 1 ? 2*bar_style[s]-(bars-1) : 0;
                const int cat = category[c]+offset;
                if (end == baseline) {
                    if (chart.series.size() > 1) return {};
                    const Point p = chart.horizontal ? Point{baseline,cat} : Point{cat,baseline};
                    owners[index(p)] |= bit; tiny[index(p)] = true;
                }
                for (int p = baseline+step; p != end+step && end != baseline; p += step) {
                    if (!work.spend()) return {};
                    const Point point = chart.horizontal ? Point{p,cat} : Point{cat,p};
                    owners[index(point)] |= bit;
                }
            }
            continue;
        }
        const int style = line_style[s];
        auto segment = [&](Point from,Point to) {
            if (from.x != to.x && from.y != to.y) return false;
            const Point step{(to.x>from.x)-(to.x<from.x),(to.y>from.y)-(to.y<from.y)};
            for (Point p = from;; p = p+step) {
                if (!work.spend()) return false;
                const auto i = index(p); owners[i] |= bit;
                if (p != from) line_bits[static_cast<std::size_t>(style)][i] |= direction_bits(Point{}-step);
                if (p != to) line_bits[static_cast<std::size_t>(style)][i] |= direction_bits(step);
                if (p == to) break;
            }
            return true;
        };
        auto point = [&](std::size_t c) { return chart.horizontal ? Point{project(chart.series[s].values[c]),category[c]} : Point{category[c],project(chart.series[s].values[c])}; };
        if (chart.categories.size() == 1) {
            const auto p = point(0); owners[index(p)] |= bit;
            line_bits[static_cast<std::size_t>(style)][index(p)] = chart.horizontal ? kUp|kDown : kLeft|kRight;
        }
        for (std::size_t c = 1; c < chart.categories.size(); ++c) {
            const auto a = point(c-1),b = point(c);
            const int middle = chart.horizontal ? (a.y+b.y)/2 : (a.x+b.x)/2;
            const Point first = chart.horizontal ? Point{a.x,middle} : Point{middle,a.y};
            const Point second = chart.horizontal ? Point{b.x,middle} : Point{middle,b.y};
            if (!segment(a,first) || !segment(first,second) || !segment(second,b)) return {};
        }
    }
    std::set<std::uint32_t> composites;
    // RFC 0031 E5: these two owners-vector walks are area-bounded and already
    // prepaid by the area fees above (and the <=255-entry ordered-set cost by
    // the parse fixed term); only the grammar booleans remain.
    for (std::size_t at = 0; at < owners.size(); ++at) {
        const auto mask = owners[at];
        const bool axis = chart.horizontal ? static_cast<int>(at%static_cast<std::size_t>(width)) == left :
            static_cast<int>(at/static_cast<std::size_t>(width)) == bottom;
        if (shared_axis && axis && std::popcount(mask) > 1) return {};
        if (std::popcount(mask) > 1) {
            composites.insert(mask);
            if (composites.size() > 255) return {};
        }
    }
    // More primary line styles need their separate ownership/geometry gate.
    if (composites.size() > 1 && lines > 2) return {};
    if (composites.size() == 1 && !shared_axis) {
        // The same visible mark must never mean both a single data line on an
        // axis and a different multi-series ownership set elsewhere.
        for (std::size_t i = 0; i < owners.size(); ++i) {
            const auto mask = owners[i]; if (std::popcount(mask) != 1) continue;
            const auto series = static_cast<std::size_t>(std::countr_zero(mask));
            if (chart.series[series].bar || line_style[series] != 0) continue;
            const int x = static_cast<int>(i%static_cast<std::size_t>(width)),y = static_cast<int>(i/static_cast<std::size_t>(width));
            if (chart.horizontal ? x == left : y == bottom) return {};
        }
    }
    struct CompositeKey { std::uint32_t owners; std::string marker,text; };
    std::vector<CompositeKey> composite_keys;
    composite_keys.reserve(composites.size());
    if (!composites.empty()) for (std::size_t n = 0; n < chart.series.size(); ++n) {
        if (!work.spend(chart.series[n].name.size()) || chart.series[n].name.find(" + ") != std::string::npos) return {};
        for (std::size_t previous = 0; previous < n; ++previous) {
            if (!work.spend(chart.series[n].name.size()+1)) return {};
            if (chart.series[n].name == chart.series[previous].name) return {};
        }
    }
    int final_width = width;
    for (const auto mask : composites) {
        if (!work.spend(4)) return {};
        std::string marker = "⊕";
        if (composites.size() > 1) {
            // These are finite visible identities, not a mask encoding. Their
            // complete source-ordered legend defines every marker's owners.
            const auto scalar = 0x2801u+static_cast<unsigned>(composite_keys.size());
            marker = {static_cast<char>(0xe0u|(scalar>>12)),static_cast<char>(0x80u|((scalar>>6)&0x3fu)),
                      static_cast<char>(0x80u|(scalar&0x3fu))};
        }
        std::string key = marker+" "; bool first = true;
        for (std::size_t owner = 0; owner < chart.series.size(); ++owner) {
            if (!work.spend()) return {};
            if (!(mask & (std::uint32_t{1} << owner))) continue;
            if (!work.spend(chart.series[owner].name.size()+3)) return {};
            if (!first) key += " + ";
            key += chart.series[owner].name; first = false;
        }
        if (composites.size() > 1) final_width = std::max(final_width,label_width(key));
        composite_keys.push_back({mask,std::move(marker),std::move(key)});
    }
    if (composites.size() > 1 || shared_axis || named_singleton || (chart.show_data && !compact_values)) {
        // Footer capacity can grow without moving any source sample or axis.
        // Legacy nonshared zero/one-composite layouts retain their old width.
        for (const auto& series : chart.series) {
            if (!work.spend(series.name.size()+4)) return {};
            final_width = std::max(final_width,label_width(series.name)+2);
        }
        for (std::size_t owner = 0; owner < chart.series.size(); ++owner)
            for (std::size_t category = 0; category < chart.categories.size(); ++category) if (disclosures[owner][category]) {
                const auto text = chart.series[owner].name+" "+chart.categories[category]+"="+numeric(chart.series[owner].values[category]);
                if (!work.spend(text.size())) return {};
                final_width = std::max(final_width,label_width(text));
            }
    }
    std::vector<std::pair<Point,std::string>> value_tokens;
    std::vector<bool> value_overlay;
    if (compact_values) {
        if (!work.spend(area)) return {};
        value_overlay.assign(area,false);
        std::set<std::pair<int,int>> reserved;
        for (std::size_t c = 0; c < chart.categories.size(); ++c) {
            if (!work.spend(32)) return {};
            const int end = project(chart.series.front().values[c]),span = std::abs(end-baseline);
            const auto text = numeric(chart.series.front().values[c]);
            if (!work.spend(text.size()) || text.empty() || span == 0) return {};
            const int text_width = label_width(text);
            const bool external = chart.show_data_outside || (chart.horizontal && text_width >= span);
            const Point first = chart.horizontal ? Point{external ? right+2 : end-text_width+1,category[c]} :
                Point{category[c],external ? top-1 : end};
            if (first.x < 0 || first.y < 0 || first.y >= height || text_width > kWidthLimit || first.x+text_width > kWidthLimit) return {};
            int replaced = 0;
            // numeric() emits ASCII round-trip tokens: each byte is one cell.
            for (int n = 0; n < text_width; ++n) {
                if (!work.spend(32)) return {};
                const Point at{first.x+n,first.y};
                if (!reserved.emplace(at.x,at.y).second ||
                    (at.x == left && at.y >= top && at.y <= bottom) ||
                    (at.y == bottom && at.x >= left && at.x <= right)) return {};
                if (at.x >= width) continue;
                const auto i = index(at);
                if (!owners[i]) continue;
                const bool own_sample = chart.horizontal ? at.y == category[c] && at.x > std::min(end,baseline) && at.x <= std::max(end,baseline) :
                    at.x == category[c] && at.y >= std::min(end,baseline) && at.y < std::max(end,baseline);
                if (external || owners[i] != 1 || !own_sample) return {};
                value_overlay[i] = true; ++replaced;
            }
            if (replaced >= span) return {};
            final_width = std::max(final_width,first.x+text_width);
            value_tokens.emplace_back(first,text);
        }
    }
    const int final_height = height+static_cast<int>(composites.size());
    if (final_width > kWidthLimit || final_height > kHeightLimit ||
        !work.spend(static_cast<std::size_t>(final_width*final_height-width*height))) return {};
    Canvas canvas(final_width,final_height,work);
    int title_y = 0;
    if (!chart.title.empty() && !canvas.text({0,title_y++},chart.title)) return {};
    const auto& top_label = chart.horizontal ? chart.x_title : chart.y_title;
    if (!top_label.empty() && !canvas.text({0,title_y},top_label)) return {};
    if (!canvas.segment({left,top},{left,bottom}) || !canvas.segment({left,bottom},{right,bottom})) return {};
    auto axis_tick = [&](Point p,unsigned extra) {
        auto& c = canvas.at(p); c.bits |= extra;
        return canvas.put(p,line_glyph(c.bits));
    };
    for (int n = 0; n <= 4; ++n) {
        const int coordinate = project(tick_values[static_cast<std::size_t>(n)]);
        const auto& text = tick_labels[static_cast<std::size_t>(n)];
        if (chart.horizontal) {
            if (!axis_tick({coordinate,bottom},kLeft|kRight|kDown) ||
                !canvas.text({coordinate-static_cast<int>(text.size())/2,bottom+1},text)) return {};
        } else if (!axis_tick({left,coordinate},kUp|kDown|kLeft) ||
                   !canvas.text({left-1-static_cast<int>(text.size()),coordinate},text)) return {};
    }
    if (!axis_tick({left,bottom},kUp|kRight|kLeft|kDown)) return {};
    for (std::size_t c = 0; c < chart.categories.size(); ++c) {
        const auto& label = chart.categories[c]; const int label_size = label_width(label);
        if (chart.horizontal) {
            if (!axis_tick({left,category[c]},kUp|kDown|kRight) || !canvas.text({left-1-label_size,category[c]},label)) return {};
        } else if (!axis_tick({category[c],bottom},kLeft|kRight|kDown) ||
                   !canvas.text({category[c]-(label_size-1)/2,bottom+1},label)) return {};
    }
    if (reverse_singleton_x) {
        // A singleton sample belongs to the first endpoint; the unused last
        // endpoint still has a physical tick and label on the authored axis.
        const auto& label = reverse_endpoints[1];
        const int at = right;
        if (
            !axis_tick({at,bottom},kLeft|kRight|kDown) ||
            !canvas.text({at-(label_width(label)-1)/2,bottom+1},label)) return {};
    }
    const auto& bottom_label = chart.horizontal ? chart.y_title : chart.x_title;
    if (!bottom_label.empty() && !canvas.text({0,axis_footer},bottom_label)) return {};
    if (shared_axis) {
        for (int y = top; y <= bottom; ++y) {
            const Point p{left,y}; if (!canvas.put(p,data_line_glyph(canvas.at(p).bits,2))) return {};
        }
        for (int x = left+1; x <= right; ++x) {
            const Point p{x,bottom}; if (!canvas.put(p,data_line_glyph(canvas.at(p).bits,2))) return {};
        }
    }
    for (std::size_t i = 0; i < owners.size(); ++i) {
        if (!work.spend()) return {};
        const auto mask = owners[i]; if (!mask || (!value_overlay.empty() && value_overlay[i])) continue;
        const Point p{static_cast<int>(i%static_cast<std::size_t>(width)),static_cast<int>(i/static_cast<std::size_t>(width))};
        std::string_view glyph;
        if (std::popcount(mask) > 1) {
            std::size_t lo = 0,hi = composite_keys.size();
            while (lo < hi) {
                if (!work.spend()) return {};
                const auto middle = lo+(hi-lo)/2;
                if (mask < composite_keys[middle].owners) hi = middle;
                else if (mask > composite_keys[middle].owners) lo = middle+1;
                else { glyph = composite_keys[middle].marker; break; }
            }
            if (glyph.empty()) return {};
        }
        else {
            const auto s = static_cast<std::size_t>(std::countr_zero(mask));
            if (chart.series[s].bar) {
                // Only the reviewed single-bar auto-domain profile shares a
                // negative endpoint with a category-axis downport. Preserve
                // both axis connections instead of painting the tick away.
                const bool shared_tick = !chart.horizontal && p.y == bottom && chart.series.size() == 1 && minimum < 0;
                glyph = shared_tick ? "⊕" : (tiny[i] ? "▁" : kFills[static_cast<std::size_t>(bar_style[s])]);
            }
            else {
                const bool axis_overlap = chart.horizontal ? p.x == left : p.y == bottom;
                const int style = line_style[s];
                if (reversed_numeric_x && (p.x == left || p.y == bottom))
                    glyph = data_line_glyph(line_bits[static_cast<std::size_t>(style)][i]|canvas.at(p).bits,style);
                else if (shared_axis && axis_overlap) glyph = data_line_glyph(line_bits[static_cast<std::size_t>(style)][i]|canvas.at(p).bits,style);
                else glyph = axis_overlap && style == 0 ? "⊕" : data_line_glyph(line_bits[static_cast<std::size_t>(style)][i],style);
            }
        }
        if (!canvas.put(p,glyph)) return {};
    }
    for (const auto& [at,text] : value_tokens) if (!canvas.text(at,text)) return {};
    footer = legend_start;
    if (chart.series.size() > 1 || named_singleton) for (std::size_t s = 0; s < chart.series.size(); ++s) {
        const auto glyph = chart.series[s].bar ? kFills[static_cast<std::size_t>(bar_style[s])] : data_line_glyph(kLeft|kRight,line_style[s]);
        if (!canvas.text({0,footer++},std::string(glyph)+" "+chart.series[s].name)) return {};
    }
    for (const auto& key : composite_keys)
        if (label_width(key.text) > final_width || !canvas.text({0,footer++},key.text)) return {};
    for (std::size_t s = 0; s < chart.series.size(); ++s) for (std::size_t c = 0; c < chart.categories.size(); ++c) if (disclosures[s][c]) {
        const auto text = chart.series[s].name+" "+chart.categories[c]+"="+numeric(chart.series[s].values[c]);
        if (label_width(text) > final_width || !canvas.text({0,footer++},text)) return {};
    }
    auto result = chart_lines(canvas); if (result.empty()) return {}; return result;
}
} // namespace
std::optional<std::vector<std::string>> charts_layout(std::string_view source,Work& work) {
    const auto chart = parse_chart(source,work);
    if (!chart) return {};
    return chart->pie ? pie_chart(*chart,work) : xy_chart(*chart,work);
}
} // namespace loom::ui::mermaid::detail
