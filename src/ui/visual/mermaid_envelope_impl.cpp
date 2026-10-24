module;

module loom.ui.visual.mermaid;

import std;

namespace loom::ui::mermaid::detail {
namespace {

constexpr std::size_t kFrontmatterLimit = 16u * 1024u;
constexpr std::size_t kDescriptionLimit = 8u * 1024u;
constexpr std::size_t kTitleLimit = 1024u;
constexpr std::size_t kInitLimit = 4u * 1024u;
constexpr std::size_t kInitTotalLimit = 8u * 1024u;
constexpr std::size_t kConfigEntryLimit = 128u;
constexpr std::size_t kConfigDepthLimit = 4u;

// RFC 0031 E5 coarse boundary fees. The entry fee is charged before the
// vector<Line> allocation and dominates the fixed struct charge plus the
// whole split_lines scan. The frontmatter fee is charged only on the
// has_frontmatter path; it dominates the block collection/event/path/value
// copies at 8/frontmatter-byte plus the <=8192-byte init raw/json copies.
constexpr std::size_t kEnvelopeEntryFixed = 3072u;
constexpr std::size_t kEnvelopeEntryPerByte = 3u;
constexpr std::size_t kEnvelopeFrontmatterFixed = 1024u;
constexpr std::size_t kEnvelopeFrontmatterPerByte = 8u;

struct Line {
    std::string_view raw;   // excluding the trailing newline and CR
    std::string_view text;  // raw after leading spaces
    std::size_t start = 0;  // byte offset of raw
    std::size_t next = 0;   // byte offset after the newline
    int indent = 0;
    std::size_t number = 0;
    bool blank = false;
    bool comment = false;
    bool tab_indent = false;
};

std::string_view trim_ws(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
    return value;
}

std::string_view strip_comment(std::string_view value) {
    char quote = 0;
    for (std::size_t n = 0; n < value.size(); ++n) {
        const char c = value[n];
        if (quote) { if (c == quote) quote = 0; continue; }
        if (c == '\'' || c == '"') { quote = c; continue; }
        if (c == '#' && (n == 0 || value[n - 1] == ' ' || value[n - 1] == '\t')) return trim_ws(value.substr(0, n));
    }
    return value;
}

bool scalar_form(std::string_view value, EnvelopeScalarForm& form) {
    if (value.starts_with("'") || value.starts_with("\"")) { form = EnvelopeScalarForm::Quoted; return true; }
    if (value == "|" || value == "|-") { form = EnvelopeScalarForm::Literal; return true; }
    if (value == ">" || value == ">-") { form = EnvelopeScalarForm::Folded; return true; }
    form = EnvelopeScalarForm::Plain;
    return true;
}

bool block_form(EnvelopeScalarForm form) {
    return form == EnvelopeScalarForm::Literal || form == EnvelopeScalarForm::Folded;
}

// RFC 0031 E5: line splitting is an O(source) scan; the single entry fee in
// parse_envelope dominates every former per-line Work charge.
bool split_lines(std::string_view source, std::vector<Line>& out) {
    std::size_t at = 0;
    std::size_t number = 1;
    while (at < source.size()) {
        const auto newline = source.find('\n', at);
        const auto end = newline == std::string_view::npos ? source.size() : newline;
        auto raw = source.substr(at, end - at);
        if (raw.ends_with('\r')) raw.remove_suffix(1);
        Line line;
        line.raw = raw;
        line.start = at;
        line.next = newline == std::string_view::npos ? source.size() : newline + 1u;
        line.number = number++;
        while (static_cast<std::size_t>(line.indent) < raw.size() && raw[static_cast<std::size_t>(line.indent)] == ' ') ++line.indent;
        if (static_cast<std::size_t>(line.indent) < raw.size() && raw[static_cast<std::size_t>(line.indent)] == '\t') line.tab_indent = true;
        line.text = raw.substr(static_cast<std::size_t>(line.indent));
        line.blank = line.text.empty();
        line.comment = !line.blank && line.text.front() == '#';
        out.push_back(line);
        at = line.next;
        if (newline == std::string_view::npos) break;
    }
    if (source.empty()) out.clear();
    return true;
}

void copy_string(std::string& out, std::string_view value) {
    out.assign(value);
}

bool collect_block(const std::vector<Line>& lines, std::size_t& index, int parent_indent,
                   std::string& out, bool deindent = false) {
    std::string block;
    std::size_t at = index + 1u;
    std::size_t first = at;
    std::size_t common = std::numeric_limits<std::size_t>::max();
    for (; at < lines.size(); ++at) {
        const auto& line = lines[at];
        if (!line.blank && line.indent <= parent_indent) break;
        if (line.tab_indent) return false;
        if (!line.blank) common = std::min(common, static_cast<std::size_t>(line.indent));
        block.append(line.raw);
        block.push_back('\n');
    }
    if (deindent && common != std::numeric_limits<std::size_t>::max()) {
        std::string stripped;
        for (std::size_t n = first; n < at; ++n) {
            const auto& line = lines[n];
            const auto skip = std::min(common, line.raw.size());
            stripped.append(line.raw.substr(skip));
            stripped.push_back('\n');
        }
        block = std::move(stripped);
    }
    // Leave the caller's line cursor on the header so its ++ lands on the
    // first line after the block (config or the closing fence).
    index = at == 0 ? 0 : at - 1;
    out = std::move(block);
    return true;
}

bool find_directive_end(std::string_view source, std::size_t begin, std::size_t& end) {
    const auto close = source.find("}%%", begin);
    if (close == std::string_view::npos) return false;
    end = close + 3u;
    return true;
}

bool directive_tail_blank(std::string_view source,std::size_t begin,std::size_t end) {
    const auto newline = source.find('\n',begin);
    const auto stop = newline == std::string_view::npos ? source.size() : newline;
    for (std::size_t at = end; at < stop; ++at) {
        if (source[at] != ' ' && source[at] != '\t' && source[at] != '\r') return false;
    }
    return true;
}

std::string_view directive_json(std::string_view raw) {
    auto body = raw;
    if (body.starts_with("%%{")) body.remove_prefix(3);
    if (body.ends_with("}%%")) body.remove_suffix(3);
    body = trim_ws(body);
    if (body.starts_with("init:")) body.remove_prefix(5);
    return trim_ws(body);
}

} // namespace

std::optional<Envelope> parse_envelope(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    // Entry boundary fee: charged before the vector<Line> allocation/scan.
    if (source.size() > (std::numeric_limits<std::size_t>::max() - kEnvelopeEntryFixed) /
                            kEnvelopeEntryPerByte ||
        !work.spend(kEnvelopeEntryFixed + kEnvelopeEntryPerByte * source.size())) return {};

    std::vector<Line> lines;
    split_lines(source, lines);
    if (lines.empty()) return {};

    Envelope envelope;
    std::size_t first_body_line = 0;
    std::size_t close_line = 0;
    bool has_frontmatter = lines[0].indent == 0 && lines[0].raw == "---";
    if (has_frontmatter) {
        for (std::size_t n = 1; n < lines.size(); ++n) {
            if (lines[n].indent == 0 && lines[n].raw == "---") { close_line = n; break; }
        }
        if (close_line == 0) return {};
        // Frontmatter boundary fee, only on the has_frontmatter path, charged
        // before any block collection/event copy. The init term is paid only
        // when the source actually contains a leading directive (the raw/json
        // copies are charged at the kInitTotalLimit cap, as their total is
        // known only after those copies run).
        const std::size_t frontmatter_bytes = lines[close_line].start;
        const bool has_init_directive = source.find("%%{") != std::string_view::npos;
        constexpr std::size_t kInitCopyTerm = 2u * kInitTotalLimit;
        const std::size_t init_term = has_init_directive ? kInitCopyTerm : 0u;
        if (frontmatter_bytes >
                (std::numeric_limits<std::size_t>::max() - kEnvelopeFrontmatterFixed - init_term) /
                    kEnvelopeFrontmatterPerByte ||
            !work.spend(kEnvelopeFrontmatterFixed + init_term +
                        kEnvelopeFrontmatterPerByte * frontmatter_bytes)) return {};
        envelope.has_frontmatter = true;
        envelope.raw_frontmatter = source.substr(lines[1].start,
            close_line > 1 ? lines[close_line].start - lines[1].start : 0u);
        if (envelope.raw_frontmatter.size() > kFrontmatterLimit) return {};
        envelope.raw_body = source.substr(lines[close_line].next);
        first_body_line = close_line + 1u;
    } else {
        envelope.raw_body = source;
        first_body_line = 0;
    }

    // Leading init directives are exposed separately; effective_body starts
    // after them. Families that do not consume leading init use raw_body.
    std::size_t effective = first_body_line;
    bool skipping = true;
    while (skipping && effective < lines.size()) {
        const auto& line = lines[effective];
        if (line.blank || line.comment) { ++effective; continue; }
        if (line.text.starts_with("%%{")) {
            std::size_t end = 0;
            if (!find_directive_end(source, line.start, end)) return {};
            if (end - line.start > kInitLimit) return {};
            if (!directive_tail_blank(source,line.start,end)) return {};
            while (effective < lines.size() && lines[effective].start < end) ++effective;
            continue;
        }
        skipping = false;
    }
    envelope.effective_body = effective < lines.size() ? source.substr(lines[effective].start) : std::string_view{};

    // Extract every %%{...}%% directive in the body (not frontmatter).
    for (std::size_t n = first_body_line; n < lines.size(); ++n) {
        const auto& line = lines[n];
        if (line.blank || !line.text.starts_with("%%{")) continue;
        std::size_t end = 0;
        if (!find_directive_end(source, line.start, end)) return {};
        if (end - line.start > kInitLimit) return {};
        if (!directive_tail_blank(source,line.start,end)) return {};
        EnvelopeInit init;
        copy_string(init.raw, source.substr(line.start, end - line.start));
        const auto json = directive_json(init.raw);
        copy_string(init.json, json);
        init.begin = line.start;
        init.end = end;
        envelope.init.push_back(std::move(init));
        if (envelope.init.size() > 16u) return {};
        std::size_t total = 0;
        for (const auto& item : envelope.init) total += item.raw.size();
        if (total > kInitTotalLimit) return {};
        while (n + 1u < lines.size() && lines[n + 1u].start < end) ++n;
    }

    if (!has_frontmatter) return envelope;

    int root_indent = 0;
    for (std::size_t n = 1; n < close_line; ++n) {
        if (lines[n].blank || lines[n].comment) continue;
        root_indent = lines[n].indent;
        break;
    }
    std::vector<std::pair<int, std::string>> config_stack;
    std::vector<std::string> seen_paths;
    bool config_seen = false;
    bool title_seen = false;
    bool description_seen = false;
    bool in_config = false;

    for (std::size_t n = 1; n < close_line; ++n) {
        const auto& line = lines[n];
        EnvelopeEvent event;
        event.line = line.number;
        event.indent = line.indent;
        if (line.blank) { event.blank = true; envelope.events.push_back(std::move(event)); continue; }
        if (line.comment) { event.comment = true; copy_string(event.value, line.text); envelope.events.push_back(std::move(event)); continue; }
        if (line.tab_indent) return {};
        if (line.indent == root_indent) {
            in_config = false;
            config_stack.clear();
            const auto colon = line.raw.find(':');
            if (colon == std::string_view::npos) return {};
            const auto key = trim_ws(line.raw.substr(0, colon));
            auto value = line.raw.substr(colon + 1u);
            if (!value.empty() && value.front() != ' ' && value.front() != '\t') return {};
            const auto raw_value = trim_ws(value);
            const auto structural = strip_comment(raw_value);
            event.top_level = true;
            copy_string(event.path, key);
            if (key == "title" || key == "description" || key == "config") {
                for (const auto& seen : seen_paths) if (seen == key) return {};
                seen_paths.emplace_back(key);
            }
            EnvelopeScalarForm form = EnvelopeScalarForm::Plain;
            if (!scalar_form(structural, form)) return {};
            event.form = form;
            if (key == "config") {
                if (!structural.empty()) return {};
                config_seen = true;
                in_config = true;
                event.namespace_entry = true;
                envelope.events.push_back(std::move(event));
                config_stack.emplace_back(root_indent, std::string{});
                continue;
            }
            std::string text;
            const bool description_block = key == "description" && (structural.empty() || block_form(form));
            if (description_block) {
                if (!collect_block(lines, n, root_indent, text)) return {};
            } else if (block_form(form)) {
                return {};
            } else copy_string(text, raw_value);
            if (key == "description") {
                if (description_seen || text.size() > kDescriptionLimit) return {};
                description_seen = true;
                EnvelopeScalar scalar;
                scalar.text = text; scalar.form = form; scalar.line = line.number;
                copy_string(scalar.header, raw_value);
                envelope.description = std::move(scalar);
                copy_string(event.value, text);
            } else if (key == "title") {
                if (title_seen || text.size() > kTitleLimit) return {};
                title_seen = true;
                EnvelopeScalar scalar;
                scalar.text = text; scalar.form = form; scalar.line = line.number;
                copy_string(scalar.header, raw_value);
                envelope.title = std::move(scalar);
                copy_string(event.value, text);
            } else {
                copy_string(event.value, text);
            }
            envelope.events.push_back(std::move(event));
            continue;
        }

        if (!in_config || !config_seen) return {};
        const auto colon = line.raw.find(':');
        if (colon == std::string_view::npos || colon < static_cast<std::size_t>(line.indent)) return {};
        const auto key = trim_ws(line.raw.substr(static_cast<std::size_t>(line.indent), colon - static_cast<std::size_t>(line.indent)));
        auto value = line.raw.substr(colon + 1u);
        if (!value.empty() && value.front() != ' ' && value.front() != '\t') return {};
        const auto raw_value = trim_ws(value);
        const auto structural = strip_comment(raw_value);
        if (key.empty() || key.find(' ') != std::string_view::npos) return {};
        while (!config_stack.empty() && line.indent <= config_stack.back().first) config_stack.pop_back();
        if (config_stack.empty()) return {};
        const auto parent = config_stack.back().second;
        std::string path = parent.empty() ? std::string(key) : parent + "." + std::string(key);
        if (std::ranges::find(seen_paths, path) != seen_paths.end()) return {};
        seen_paths.push_back(path);
        if (static_cast<std::size_t>(std::count(path.begin(), path.end(), '.')) >= kConfigDepthLimit) return {};
        event.path = path;
        EnvelopeScalarForm form = EnvelopeScalarForm::Plain;
        if (!scalar_form(structural, form)) return {};
        event.form = form;
        if (structural.empty()) {
            event.namespace_entry = true;
            config_stack.emplace_back(line.indent, path);
        } else {
            std::string text;
            if (block_form(form)) {
                if (!collect_block(lines, n, line.indent, text, true)) return {};
            } else copy_string(text, raw_value);
            copy_string(event.value, text);
            envelope.config.push_back(event);
            if (envelope.config.size() > kConfigEntryLimit) return {};
        }
        envelope.events.push_back(std::move(event));
    }
    return envelope;
}

} // namespace loom::ui::mermaid::detail
