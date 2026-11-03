// RFC 0030 D1a: bounded Class direct styles and owned semantic row extraction.
module;
#include <ftxui/screen/string.hpp>
module loom.ui.visual.mermaid;
import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {
std::string ascii_lower(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const auto c : value)
        result.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
    return result;
}

std::optional<SourceStyleColor> source_color(std::string_view raw) {
    const auto value = ascii_lower(raw);
    SourceStyleColor result;
    if (value == "white") {
        result.rgb = {255u, 255u, 255u};
        return result;
    }
    if (value.empty() || value.front() != '#' ||
        (value.size() != 4u && value.size() != 5u && value.size() != 7u && value.size() != 9u))
        return {};
    const auto digit = [](char c) -> int {
        return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
    };
    const bool short_form = value.size() <= 5u;
    const std::size_t components = (value.size() - 1u) / (short_form ? 1u : 2u);
    std::array<int, 4> rgba{0, 0, 0, 255};
    for (std::size_t n = 0; n < components; ++n) {
        const int a = digit(value[1u + n * (short_form ? 1u : 2u)]);
        if (a < 0) return {};
        int b = a;
        if (!short_form) {
            b = digit(value[2u + n * 2u]);
            if (b < 0) return {};
        }
        rgba[n] = a * 16 + b;
    }
    if (rgba[3] != 0 && rgba[3] != 255) return {};
    if (rgba[3] == 0) {
        result.transparent = true;
        return result;
    }
    for (std::size_t n = 0; n < 3u; ++n)
        result.rgb[n] = static_cast<unsigned char>(rgba[n]);
    // Integer channel ratios keep exact hue boundaries (for example #f96 at
    // 20 degrees) from rounding into the previous semantic role.
    const int high = std::max({rgba[0], rgba[1], rgba[2]}),
              low = std::min({rgba[0], rgba[1], rgba[2]});
    const int delta = high - low, sum = high + low, denominator = 255 - std::abs(sum - 255);
    if (delta == 0 || delta * 100 < 15 * denominator || sum < 51) return result;
    double hue = high == rgba[0]   ? 60.0 * (rgba[1] - rgba[2]) / delta
                 : high == rgba[1] ? 60.0 * (rgba[2] - rgba[0] + 2 * delta) / delta
                                   : 60.0 * (rgba[0] - rgba[1] + 4 * delta) / delta;
    if (hue < 0.0) hue += 360.0;
    result.role = hue < 20.0 || hue >= 345.0 ? SemanticColorRole::Red
                  : hue < 45.0               ? SemanticColorRole::Orange
                  : hue < 70.0               ? SemanticColorRole::Yellow
                  : hue < 165.0              ? SemanticColorRole::Green
                  : hue < 195.0              ? SemanticColorRole::Cyan
                  : hue < 255.0              ? SemanticColorRole::Blue
                                             : SemanticColorRole::Purple;
    return result;
}

std::string_view role_name(SemanticColorRole role) {
    switch (role) {
    case SemanticColorRole::Red:
        return "Red";
    case SemanticColorRole::Orange:
        return "Orange(shared warning)";
    case SemanticColorRole::Yellow:
        return "Yellow(shared warning)";
    case SemanticColorRole::Green:
        return "Green";
    case SemanticColorRole::Cyan:
        return "Cyan(shared info)";
    case SemanticColorRole::Blue:
        return "Blue(shared info)";
    case SemanticColorRole::Purple:
        return "Purple(surface tint)";
    default:
        return "Neutral";
    }
}

bool checked_add(std::size_t& total, std::size_t value) {
    if (value > std::numeric_limits<std::size_t>::max() - total) return false;
    total += value;
    return true;
}

bool checked_product(std::size_t& total, std::size_t count, std::size_t size) {
    if (size && count > std::numeric_limits<std::size_t>::max() / size) return false;
    return checked_add(total, count * size);
}

bool parse_style_properties(std::string_view rest, ClassStyleProperties& properties) {
    unsigned seen = 0u;
    while (!rest.empty()) {
        const auto separator = rest.find_first_of(",;");
        const auto component = trim_relation(rest.substr(0, separator));
        if (component.empty()) return false;
        const auto colon = component.find(':');
        if (colon == std::string_view::npos) return false;
        const auto property = ascii_lower(trim_relation(component.substr(0, colon))),
                   value = ascii_lower(trim_relation(component.substr(colon + 1u)));
        const unsigned bit = property == "fill"           ? 1u
                             : property == "stroke"       ? 2u
                             : property == "color"        ? 4u
                             : property == "stroke-width" ? 8u
                                                          : 0u;
        if (!bit || (seen & bit) || value.empty()) return false;
        seen |= bit;
        if (bit == 8u) {
            auto number = std::string_view(value);
            if (number.ends_with("px")) number.remove_suffix(2u);
            double width = 0.0;
            const auto parsed =
                std::from_chars(number.data(), number.data() + number.size(), width);
            if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() ||
                !std::isfinite(width) || width < 0.0 || width > 64.0)
                return false;
            properties.width = width;
        } else {
            const auto color = source_color(value);
            if (!color || (bit != 1u && color->transparent)) return false;
            if (bit == 1u)
                properties.fill = *color;
            else if (bit == 2u)
                properties.stroke = *color;
            else
                properties.color = *color;
        }
        if (separator == std::string_view::npos) break;
        rest = trim_relation(rest.substr(separator + 1u));
    }
    if (!seen) return false;
    return true;
}

bool style_identifier(std::string_view value) {
    if (value.empty() || value.size() > 64u) return false;
    for (std::size_t n = 0; n < value.size(); ++n) {
        const auto c = value[n];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' ||
              (n > 0u && c >= '0' && c <= '9')))
            return false;
    }
    return true;
}

std::optional<std::pair<std::string_view, std::size_t>>
style_physical_record(std::string_view line, std::string_view source, Work& work) {
    if (source.size() > kSourceLimit || line.size() > 1024u ||
        !work.spend(source.size() * 2u + line.size() * 24u + 1024u))
        return {};
    const auto offset = static_cast<std::size_t>(line.data() - source.data());
    if (offset >= source.size() || line.size() > source.size() - offset) return {};
    const auto previous = source.rfind('\n', offset), next = source.find('\n', offset);
    const auto begin = previous == std::string_view::npos ? 0u : previous + 1u;
    auto physical =
        source.substr(begin, (next == std::string_view::npos ? source.size() : next) - begin);
    if (physical.ends_with('\r') && next != std::string_view::npos) physical.remove_suffix(1u);
    if (physical.size() > 1024u || trim_relation(physical) != line) return {};
    for (const auto c : physical)
        if ((static_cast<unsigned char>(c) < 0x20u && c != '\t') ||
            static_cast<unsigned char>(c) == 0x7fu)
            return {};
    return std::pair{physical, begin};
}

bool ensure_class_styles(RelationDiagram& diagram, Work& work) {
    if (diagram.direct_styles) return true;
    if (!work.spend(4u * sizeof(ClassDirectStyles) + 64u * sizeof(ClassDirectStyle) + 256u))
        return false;
    diagram.direct_styles = std::make_unique<ClassDirectStyles>();
    diagram.direct_styles->declarations.reserve(64u);
    return true;
}

}  // namespace

bool source_style_properties(std::string_view raw, ClassStyleProperties& properties, Work& work) {
    if (raw.size() > 1024u || !work.spend(raw.size() * 24u + 1024u)) return false;
    return parse_style_properties(raw, properties);
}

std::string_view source_style_role_name(SemanticColorRole role) {
    return role_name(role);
}

bool class_direct_style(std::string_view line, std::string_view source, RelationDiagram& diagram,
                        Work& work) {
    if (line.size() > 1024u || source.size() > kSourceLimit ||
        !work.spend(source.size() * 2u + line.size() * 24u + 1024u))
        return false;
    const auto offset = static_cast<std::size_t>(line.data() - source.data());
    if (offset >= source.size() || line.size() > source.size() - offset) return false;
    const auto previous = source.rfind('\n', offset), next = source.find('\n', offset);
    const auto begin = previous == std::string_view::npos ? 0u : previous + 1u;
    auto physical =
        source.substr(begin, (next == std::string_view::npos ? source.size() : next) - begin);
    if (physical.ends_with('\r') && next != std::string_view::npos) physical.remove_suffix(1u);
    if (physical.size() > 1024u || trim_relation(physical) != line) return false;
    for (const auto c : physical)
        if ((static_cast<unsigned char>(c) < 0x20u && c != '\t') ||
            static_cast<unsigned char>(c) == 0x7fu)
            return false;
    auto rest = trim_relation(line.substr(5u));
    const auto split = rest.find_first_of(" \t");
    if (split == std::string_view::npos || split == 0u || split > 64u) return false;
    const auto target = rest.substr(0, split);
    for (std::size_t n = 0; n < target.size(); ++n) {
        const char c = target[n];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' ||
              (n > 0u && c >= '0' && c <= '9')))
            return false;
    }
    ClassDirectStyle declaration;
    declaration.target = target;
    declaration.verbatim = physical;
    declaration.source_offset = begin;
    rest = trim_relation(rest.substr(split));
    if (!parse_style_properties(rest, declaration.properties)) return false;
    if (!diagram.direct_styles) {
        if (!work.spend(4u * sizeof(ClassDirectStyles) + 64u * sizeof(ClassDirectStyle) + 256u))
            return false;
        diagram.direct_styles = std::make_unique<ClassDirectStyles>();
        diagram.direct_styles->declarations.reserve(64u);
    }
    if (diagram.direct_styles->declarations.size() >= 64u) return false;
    diagram.direct_styles->declarations.push_back(std::move(declaration));
    return true;
}

bool class_style_assignment(std::string_view statement, std::string_view source,
                            std::string_view target, std::string_view name,
                            RelationDiagram& diagram, Work& work) {
    if (!style_identifier(target) || !style_identifier(name)) return false;
    const auto physical = style_physical_record(statement, source, work);
    if (!physical) return false;
    if (!ensure_class_styles(diagram, work)) return false;
    auto& assignments = diagram.direct_styles->assignments;
    if (assignments.size() >= 64u ||
        !work.spend((target.size() + name.size() + physical->first.size()) * 8u +
                    4u * sizeof(ClassStyleAssignment) + 256u))
        return false;
    if (assignments.empty()) {
        if (!work.spend(64u * 4u * sizeof(ClassStyleAssignment) + 256u)) return false;
        assignments.reserve(64u);
    }
    assignments.push_back(
        {std::string(target), std::string(name), std::string(physical->first), physical->second});
    return true;
}

bool class_css_assignment(std::string_view line, std::string_view source, RelationDiagram& diagram,
                          Work& work) {
    if (line.size() > 1024u || !work.spend(line.size() * 8u + 256u)) return false;
    auto rest = trim_relation(line.substr(8u));
    if (rest.size() < 4u || rest.front() != '"') return false;
    const auto close = rest.find('"', 1u);
    if (close == std::string_view::npos || close + 1u >= rest.size() ||
        (rest[close + 1u] != ' ' && rest[close + 1u] != '\t'))
        return false;
    const auto target = rest.substr(1u, close - 1u), name = trim_relation(rest.substr(close + 1u));
    return class_style_assignment(line, source, target, name, diagram, work);
}

namespace {
struct AssignedDefinition {
    std::string_view name, physical, properties;
    std::size_t source_offset = 0u;
    bool used = false;
};

struct AssignedLegend {
    std::size_t source_offset;
    std::string text;
};

bool resolve_assigned_class_styles(RelationDiagram& diagram,
                                   std::span<const std::string_view> validated_definitions,
                                   std::string_view source, Work& work) {
    auto& styles = *diagram.direct_styles;
    if (styles.assignments.size() > 64u || styles.declarations.size() > 64u ||
        !work.spend(64u * 4u * sizeof(AssignedDefinition) + 256u))
        return false;
    std::vector<AssignedDefinition> definitions;
    definitions.reserve(64u);
    // Only source views accepted at root by class_inactive_definition are
    // authorities. A class member may literally start with "classDef".
    for (const auto validated : validated_definitions) {
        if (definitions.size() >= 64u || !style_identifier(validated) ||
            !work.spend(source.size() * 2u + validated.size() * 8u + 2048u))
            return false;
        const auto offset = static_cast<std::size_t>(validated.data() - source.data());
        if (offset >= source.size() || validated.size() > source.size() - offset) return false;
        const auto prior = source.rfind('\n', offset), next = source.find('\n', offset);
        const auto begin = prior == std::string_view::npos ? 0u : prior + 1u;
        const auto line = trim_relation(
            source.substr(begin, (next == std::string_view::npos ? source.size() : next) - begin));
        if (line.size() <= 8u || !line.starts_with("classDef") ||
            (line[8] != ' ' && line[8] != '\t'))
            return false;
        const auto physical = style_physical_record(line, source, work);
        if (!physical) return false;
        auto rest = trim_relation(line.substr(8u));
        const auto split = rest.find_first_of(" \t");
        if (split == std::string_view::npos || rest.substr(0u, split) != validated ||
            rest.data() != validated.data())
            return false;
        definitions.push_back({validated, physical->first, trim_relation(rest.substr(split)),
                               physical->second, false});
    }
    const auto count = styles.assignments.size() + styles.declarations.size();
    if (count > 128u || !work.spend(count * 4u * sizeof(ClassDirectStyle) + 256u)) return false;
    std::vector<ClassDirectStyle> merged;
    merged.reserve(count);
    for (const auto& assignment : styles.assignments) {
        std::optional<std::size_t> selected;
        for (std::size_t n = 0; n < definitions.size(); ++n) {
            if (!work.spend(definitions[n].name.size() + assignment.class_name.size() + 32u))
                return false;
            if (definitions[n].name == assignment.class_name) {
                selected = n;
                break;
            }
        }
        if (!selected) return false;
        auto& definition = definitions[*selected];
        if (!work.spend((assignment.target.size() + assignment.class_name.size() +
                         assignment.verbatim.size()) *
                            8u +
                        definition.properties.size() * 24u + 4u * sizeof(ClassDirectStyle) + 1024u))
            return false;
        ClassDirectStyle declaration;
        declaration.target = assignment.target;
        declaration.class_name = assignment.class_name;
        declaration.verbatim = assignment.verbatim;
        declaration.source_offset = assignment.source_offset;
        if (!parse_style_properties(definition.properties, declaration.properties)) return false;
        definition.used = true;
        merged.push_back(std::move(declaration));
    }
    // Definitions cascade in assignment order. Direct declarations always apply
    // last, even when their physical source statements preceded an assignment.
    for (auto& declaration : styles.declarations) {
        if (!work.spend(64u)) return false;
        merged.push_back(std::move(declaration));
    }
    styles.declarations = std::move(merged);
    if (!work.spend(diagram.entities.size() * 4u * sizeof(ClassStyleProperties) +
                    4u * sizeof(std::vector<ClassStyleProperties>) + 256u))
        return false;
    styles.effective.resize(diagram.entities.size());
    for (auto& declaration : styles.declarations) {
        if (!work.spend(declaration.target.size() * (diagram.entities.size() + 1u) + 64u))
            return false;
        const int target = diagram.find(declaration.target);
        if (target < 0) return false;
        const auto& entity = diagram.entities[static_cast<std::size_t>(target)];
        if (entity.owner != -1 || entity.shape != EntityShape::Box) return false;
        declaration.entity = target;
        auto& effective = styles.effective[static_cast<std::size_t>(target)];
        const auto& properties = declaration.properties;
        if (properties.fill) effective.fill = properties.fill;
        if (properties.stroke) effective.stroke = properties.stroke;
        if (properties.color) effective.color = properties.color;
        if (properties.width) effective.width = properties.width;
    }
    if (!work.spend(7u * 4u * sizeof(AssignedLegend) + 8u * 4u * sizeof(std::string) + 8192u))
        return false;
    std::vector<AssignedLegend> records;
    records.reserve(7u);
    styles.legend.reserve(8u);
    const auto raw_record = [&](std::string_view raw) -> std::optional<std::string> {
        if (raw.size() > 1024u || !work.spend(raw.size() * 12u + 4096u)) return {};
        std::string text;
        text.reserve(raw.size() * 2u + 256u);
        for (const auto c : raw) {
            if (c == '\t')
                text += "\\t";
            else
                text += c;
        }
        text += " ->";
        return text;
    };
    for (std::size_t n = 0; n < styles.declarations.size(); ++n) {
        const auto& declaration = styles.declarations[n];
        bool prior = false;
        std::size_t occurrences = 0u;
        for (std::size_t k = 0; k < styles.declarations.size(); ++k) {
            const auto& other = styles.declarations[k];
            if (!work.spend(declaration.target.size() + declaration.class_name.size() + 64u))
                return false;
            if (other.target == declaration.target && other.class_name == declaration.class_name &&
                other.properties == declaration.properties) {
                ++occurrences;
                if (k < n) prior = true;
            }
        }
        if (prior) continue;
        if (records.size() >= 7u) return false;
        auto text = raw_record(declaration.verbatim);
        if (!text) return false;
        const auto append = [&](std::string_view key, std::string_view value) {
            *text += ' ';
            *text += key;
            *text += ':';
            *text += value;
        };
        if (!declaration.class_name.empty()) append("class", declaration.class_name);
        const auto& properties = declaration.properties;
        const auto& effective = styles.effective[static_cast<std::size_t>(declaration.entity)];
        if (properties.fill)
            append("fill", properties.fill->transparent ? std::string_view("none")
                                                        : role_name(properties.fill->role));
        if (properties.stroke) append("stroke", role_name(properties.stroke->role));
        if (properties.width) append("width", *properties.width >= 4.0 ? "bold" : "normal");
        if (properties.color) {
            append("fg", role_name(properties.color->role));
            if (effective.fill && !effective.fill->transparent) *text += "(not projected on fill)";
        }
        if (occurrences > 1u) {
            *text += " occurrences:";
            *text += std::to_string(occurrences);
        }
        if (ftxui::string_width(*text) > kWidthLimit) return false;
        records.push_back({declaration.source_offset, std::move(*text)});
    }
    for (const auto& definition : definitions) {
        if (!work.spend(32u)) return false;
        if (!definition.used) continue;
        if (records.size() >= 7u) return false;
        auto text = raw_record(definition.physical);
        if (!text) return false;
        *text += " definition:";
        *text += definition.name;
        *text += " (applied)";
        if (ftxui::string_width(*text) > kWidthLimit) return false;
        records.push_back({definition.source_offset, std::move(*text)});
    }
    if (!work.spend(records.size() * records.size() * 64u + 256u)) return false;
    std::ranges::sort(records, {}, &AssignedLegend::source_offset);
    for (auto& record : records)
        styles.legend.push_back(std::move(record.text));
    styles.legend.emplace_back("key: hue roles use palette; width>=4=bold border; color on fill "
                               "not projected; monochrome/ANSI16 may share shades; source TAB=\\t");
    return true;
}
}  // namespace

bool resolve_class_direct_styles(RelationDiagram& diagram, Work& work,
                                 std::span<const std::string_view> validated_definitions,
                                 std::string_view source) {
    if (!diagram.direct_styles) return true;
    auto& styles = *diagram.direct_styles;
    if (!styles.assignments.empty())
        return resolve_assigned_class_styles(diagram, validated_definitions, source, work);
    if (!work.spend(diagram.entities.size() * (sizeof(ClassStyleProperties) + 16u) +
                    8u * sizeof(std::string) + 512u))
        return false;
    styles.effective.resize(diagram.entities.size());
    styles.legend.reserve(8u);
    for (std::size_t n = 0; n < styles.declarations.size(); ++n) {
        auto& declaration = styles.declarations[n];
        if (!work.spend(declaration.target.size() * (diagram.entities.size() + 1u) + 64u))
            return false;
        const int target = diagram.find(declaration.target);
        if (target < 0) return false;
        const auto& entity = diagram.entities[static_cast<std::size_t>(target)];
        if (entity.owner != -1 || entity.shape != EntityShape::Box) return false;
        declaration.entity = target;
        auto& effective = styles.effective[static_cast<std::size_t>(target)];
        const auto& properties = declaration.properties;
        if (properties.fill) effective.fill = properties.fill;
        if (properties.stroke) effective.stroke = properties.stroke;
        if (properties.color) effective.color = properties.color;
        if (properties.width) effective.width = properties.width;
    }
    for (std::size_t n = 0; n < styles.declarations.size(); ++n) {
        const auto& declaration = styles.declarations[n];
        const auto& properties = declaration.properties;
        const auto& effective = styles.effective[static_cast<std::size_t>(declaration.entity)];
        bool prior = false;
        std::size_t occurrences = 0u;
        for (std::size_t k = 0; k < styles.declarations.size(); ++k) {
            if (!work.spend(32u + declaration.target.size())) return false;
            const auto& other = styles.declarations[k];
            if (other.target == declaration.target && other.properties == properties) {
                ++occurrences;
                if (k < n) prior = true;
            }
        }
        if (prior) continue;
        if (styles.legend.size() >= 7u || !work.spend(declaration.verbatim.size() * 8u + 4096u))
            return false;
        std::string legend;
        legend.reserve(declaration.verbatim.size() * 2u + 256u);
        for (const auto c : declaration.verbatim) {
            if (c == '\t')
                legend += "\\t";
            else
                legend += c;
        }
        legend += " ->";
        const auto append = [&](std::string_view key, std::string_view value) {
            legend += ' ';
            legend += key;
            legend += ':';
            legend += value;
        };
        if (properties.fill)
            append("fill", properties.fill->transparent ? std::string_view("none")
                                                        : role_name(properties.fill->role));
        if (properties.stroke) append("stroke", role_name(properties.stroke->role));
        if (properties.width) append("width", *properties.width >= 4.0 ? "bold" : "normal");
        if (properties.color) {
            append("fg", role_name(properties.color->role));
            if (effective.fill && !effective.fill->transparent) legend += "(not projected on fill)";
        }
        if (occurrences > 1u) {
            legend += " occurrences:";
            legend += std::to_string(occurrences);
        }
        if (ftxui::string_width(legend) > kWidthLimit) return false;
        styles.legend.push_back(std::move(legend));
    }
    styles.legend.emplace_back("key: hue roles use palette; width>=4=bold border; color on fill "
                               "not projected; monochrome/ANSI16 may share shades; source TAB=\\t");
    return true;
}

bool paint_class_direct_styles(Canvas& canvas, const RelationDiagram& diagram,
                               std::span<const RelationBox> boxes) {
    if (!diagram.direct_styles) return true;
    if (boxes.size() != diagram.entities.size()) return false;
    const auto count = canvas.cells.size();
    if (!canvas.work.spend(count * (sizeof(SemanticCellStyle) + 8u) +
                           4u * sizeof(std::vector<SemanticCellStyle>) + 256u))
        return false;
    canvas.styles = std::make_unique<std::vector<SemanticCellStyle>>(count);
    for (std::size_t n = 0; n < diagram.direct_styles->effective.size(); ++n) {
        const auto& properties = diagram.direct_styles->effective[n];
        if (!properties.fill && !properties.stroke && !properties.color && !properties.width)
            continue;
        const auto& box = boxes[n];
        if (box.w <= 0 || box.h <= 0 || !canvas.inside({box.x, box.y}) ||
            !canvas.inside({box.right(), box.bottom()}))
            return false;
        const bool filled = properties.fill && !properties.fill->transparent;
        for (int y = box.y; y <= box.bottom(); ++y)
            for (int x = box.x; x <= box.right(); ++x) {
                if (!canvas.work.spend(8u + sizeof(SemanticCellStyle))) return false;
                const Point at{x, y};
                const bool border =
                    x == box.x || x == box.right() || y == box.y || y == box.bottom();
                const bool stroke = border || canvas.at(at).bits != 0u;
                SemanticCellStyle style;
                if (!border && filled) style.fill = properties.fill->role;
                if (stroke) {
                    if (properties.stroke) style.foreground = properties.stroke->role;
                    style.bold = properties.width && *properties.width >= 4.0;
                } else if (filled)
                    style.foreground = SemanticColorRole::Neutral;
                else if (properties.color)
                    style.foreground = properties.color->role;
                (*canvas.styles)[static_cast<std::size_t>(canvas.index(at))] = style;
            }
    }
    return true;
}

std::optional<StyledRows> extract_styled_rows(const Canvas& canvas,
                                              std::span<const std::string> lines,
                                              std::size_t leading_rows, std::size_t drawing_rows,
                                              Work& work) {
    if (!canvas.styles || lines.size() > static_cast<std::size_t>(kHeightLimit) ||
        !work.spend(lines.size() * (4u * sizeof(std::vector<StyledRun>) + 32u) + 256u))
        return {};
    StyledRows result;
    result.rows.reserve(lines.size());
    int x_min = canvas.width, y_min = canvas.height, x_max = -1, y_max = -1;
    if (!work.spend(canvas.cells.size() * 4u + 64u)) return {};
    for (int y = 0; y < canvas.height; ++y)
        for (int x = 0; x < canvas.width; ++x)
            if (canvas.at({x, y}).glyph != " ") {
                x_min = std::min(x_min, x);
                y_min = std::min(y_min, y);
                x_max = std::max(x_max, x);
                y_max = std::max(y_max, y);
            }
    if (x_max < x_min || y_max < y_min ||
        drawing_rows != static_cast<std::size_t>(y_max - y_min + 1))
        return {};
    for (int y = 0; y < canvas.height; ++y)
        for (int x = 0; x < canvas.width; ++x)
            if ((x < x_min || x > x_max || y < y_min || y > y_max) &&
                (*canvas.styles)[static_cast<std::size_t>(canvas.index({x, y}))] !=
                    SemanticCellStyle{})
                return {};
    for (std::size_t y = 0; y < lines.size(); ++y) {
        const auto& line = lines[y];
        const int width = ftxui::string_width(line);
        if (width > kWidthLimit || !work.spend(line.size() * 8u + 256u)) return {};
        std::vector<StyledRun> runs;
        if (y < leading_rows || y - leading_rows >= drawing_rows) {
            runs.push_back({line, {}});
        } else {
            if (width > canvas.width ||
                !work.spend((static_cast<std::size_t>(width) + 1u) * (sizeof(StyledRun) + 16u)))
                return {};
            runs.reserve(static_cast<std::size_t>(width) + 1u);
            for (int x = 0; x < width; ++x) {
                if (!work.spend(16u)) return {};
                const Point at{x + x_min, static_cast<int>(y - leading_rows) + y_min};
                const auto& cell = canvas.at(at);
                const auto style = (*canvas.styles)[static_cast<std::size_t>(canvas.index(at))];
                if (cell.glyph.empty()) {
                    if (runs.empty() || runs.back().style != style) return {};
                    continue;
                }
                if (runs.empty() || runs.back().style != style) runs.push_back({{}, style});
                runs.back().text += cell.glyph;
            }
            if (runs.empty()) runs.push_back({{}, {}});
            std::string joined;
            for (const auto& run : runs)
                joined += run.text;
            if (joined != line) return {};
        }
        result.rows.push_back(std::move(runs));
    }
    return result;
}

namespace {
// Frozen gas footprint of NativeLayout as charged by the payload gates. ADR
// 0032 added two ER-only side fields (bool er_packed + std::size_t
// payload_prepaid), growing the real struct 88 -> 104 bytes. Those fields
// fund no allocation on any path, so the non-ER families (Block, Class/State,
// styled Flow) must keep paying the pre-ADR charge; the ER gates credit this
// exact amount via packed_payload_prepaid. Keep this constant in lockstep
// with the field set that predates ADR 0032.
constexpr std::size_t kNativeLayoutGasFootprint = 88u;
// Mirror of NativeLayout's pre-ADR-0032 field set; locks the constant's
// derivation against accidental field-set drift (the real NativeLayout adds
// the two ER-only side fields).
struct PreAdrNativeLayoutFootprint {
    std::vector<std::string> rows;
    std::optional<StyledRows> styles;
    std::size_t measured_layout_work = 0u;
    std::vector<std::uint16_t> packed_styles{};
};
static_assert(sizeof(PreAdrNativeLayoutFootprint) == kNativeLayoutGasFootprint);
static_assert(kNativeLayoutGasFootprint <= sizeof(NativeLayout),
              "charged footprint must never exceed the real struct");
}  // namespace

std::optional<std::size_t> styled_payload_bytes(const NativeLayout& layout,
                                                std::size_t source_bytes) {
    std::size_t total = kNativeLayoutGasFootprint + 256u;
    if (!checked_product(total, source_bytes, 2u) ||
        !checked_product(total, layout.rows.capacity(), sizeof(std::string)))
        return {};
    for (const auto& row : layout.rows)
        if (!checked_add(total, row.capacity())) return {};
    if (!checked_product(total, layout.packed_styles.capacity(), sizeof(std::uint16_t))) return {};
    if (layout.styles) {
        if (!checked_product(total, layout.styles->rows.capacity(), sizeof(std::vector<StyledRun>)))
            return {};
        for (const auto& row : layout.styles->rows) {
            if (!checked_product(total, row.capacity(), sizeof(StyledRun))) return {};
            for (const auto& run : row)
                if (!checked_add(total, run.text.capacity())) return {};
        }
    }
    return total;
}

bool prepay_styled_payload(NativeLayout& layout, std::size_t source_bytes, Work& work,
                           std::size_t* prepaid) {
    if (!layout.styles && layout.packed_styles.empty()) return true;
    const auto bytes = styled_payload_bytes(layout, source_bytes);
    if (!bytes) return false;
    std::size_t charge = 512u;
    if (!checked_product(charge, *bytes, 6u) || !checked_product(charge, source_bytes, 4u))
        return false;
    if (prepaid)
        charge -= std::min(charge, *prepaid);
    if (!work.spend(charge)) return false;
    return true;
}

std::size_t packed_payload_prepaid(const std::vector<std::string>& rows,
                                   const std::vector<std::uint16_t>& styles,
                                   std::size_t source_bytes) {
    // Layout-Work amount ErPaint::finish prepaid for this exact packed payload:
    // valid_packed_rows (full), prepay_styled_payload (full packed charge), and
    // finish_mermaid_payload's duplicated bookkeeping (8B byte re-scan and
    // 8*sizeof(StyledRun) run re-enumeration). The real ftxui Element fees are
    // billed by the separate ElementWork meter and are not included.
    std::size_t bytes = 0u, runs = 0u, slot = 0u;
    for (const auto& line : rows) {
        bytes += line.size();
        std::size_t cells = 0u;
        std::uint16_t previous = 0u;
        for (std::size_t byte = 0u; byte < line.size();) {
            const auto lead = static_cast<unsigned char>(line[byte]);
            const std::size_t length = lead < 0x80 ? 1u : lead < 0xe0 ? 2u
                                                      : lead < 0xf0   ? 3u
                                                                      : 4u;
            const auto occupied =
                static_cast<std::size_t>(ftxui::string_width(line.substr(byte, length)));
            byte += length;
            if (occupied == 0u) continue;
            if (slot + cells < styles.size()) {
                const auto code = styles[slot + cells];
                if (cells == 0u || code != previous) ++runs;
                previous = code;
            }
            cells += occupied;
        }
        slot += cells;
    }
    std::size_t total = rows.size() * 32u + 128u + bytes * 4u + styles.size() * 2u;
    const NativeLayout view{std::vector<std::string>(rows), std::nullopt, 0u,
                            std::vector<std::uint16_t>(styles)};
    if (const auto charged = styled_payload_bytes(view, source_bytes)) {
        std::size_t styled = 512u;
        if (checked_product(styled, *charged, 6u) &&
            checked_product(styled, source_bytes, 4u))
            total += styled;
    }
    total += bytes * 8u + runs * 8u * sizeof(StyledRun);
    return total;
}
}  // namespace loom::ui::mermaid::detail
