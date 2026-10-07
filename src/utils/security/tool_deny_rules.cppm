/// @file tool_deny_rules.cppm
/// @brief Pure permission deny-rule grammar and matcher for pre-listing tool
/// filtering. Zero internal imports so any layer can consume it.
module;

#include <cctype>

export module loom.security.tool_deny_rules;

import std;

export namespace loom::utils::tool_deny_rules {

// ============================================================
// MCP name normalization
// ============================================================

/// Normalize server/tool names for the API pattern ^[a-zA-Z0-9_-]{1,64}$:
/// every char outside [A-Za-z0-9_-] becomes '_'.
///
/// The hosted-connector prefix special case is deleted: there is no
/// hosted connector service, so nothing can produce a name with that
/// prefix, and the branch was unreachable for every real MCP server name
/// (which comes from user configuration). A prefix test that can never be
/// true is not a safety net, it is a place for a future reader to believe
/// something is handled that is not.
[[nodiscard]] inline std::string normalize_name_for_mcp(std::string_view name) {
    std::string normalized;
    normalized.reserve(name.size());
    for (const char ch : name) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        const bool allowed =
            std::isalnum(uch) != 0 || ch == '_' || ch == '-';
        normalized.push_back(allowed ? ch : '_');
    }
    return normalized;
}

// ============================================================
// MCP qualified-name parsing
// ============================================================

struct McpInfo {
    std::string server;
    std::optional<std::string> tool;
};

/// Parse a qualified name of the form "mcp__serverName__toolName".
/// Splits on "__"; requires the first segment to be exactly "mcp" and a
/// non-empty server segment. All segments after the server are rejoined with
/// "__" (double underscores inside tool names are preserved). A bare
/// "mcp__serverName" yields a nullopt tool. Returns nullopt when the input is
/// not a qualified MCP name.
///
/// Known limitation: a server name containing "__" parses incorrectly;
/// replicate, do not "fix".
[[nodiscard]] inline std::optional<McpInfo> mcp_info_from_string(
    std::string_view tool_string) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const auto pos = tool_string.find("__", start);
        if (pos == std::string_view::npos) {
            parts.emplace_back(tool_string.substr(start));
            break;
        }
        parts.emplace_back(tool_string.substr(start, pos - start));
        start = pos + 2;
    }

    if (parts.size() < 2 || parts[0] != "mcp" || parts[1].empty()) {
        return std::nullopt;
    }

    McpInfo info;
    info.server = parts[1];
    if (parts.size() > 2) {
        std::string tool_name = parts[2];
        for (std::size_t i = 3; i < parts.size(); ++i) {
            tool_name += "__";
            tool_name += parts[i];
        }
        info.tool = std::move(tool_name);
    }
    return info;
}

// ============================================================
// Permission rule parsing
// ============================================================

struct ParsedDenyRule {
    std::string tool_name;
    bool has_content = false;
};

namespace detail {

/// Apply legacy tool-name renames so old rules resolve to canonical names.
[[nodiscard]] inline std::string normalize_legacy_tool_name(
    std::string_view name) {
    if (name == "Task") return "Agent";
    if (name == "KillShell") return "TaskStop";
    if (name == "AgentOutputTool" || name == "BashOutputTool") {
        return "TaskOutput";
    }
    return std::string(name);
}

/// Index of the first occurrence of `ch` not preceded by an odd run of
/// backslashes, or npos.
[[nodiscard]] inline std::size_t find_first_unescaped_char(
    std::string_view str, char ch) noexcept {
    for (std::size_t i = 0; i < str.size(); ++i) {
        if (str[i] != ch) continue;
        int backslash_count = 0;
        std::size_t j = i;
        while (j > 0 && str[j - 1] == '\\') {
            ++backslash_count;
            --j;
        }
        if (backslash_count % 2 == 0) return i;  // unescaped
    }
    return std::string_view::npos;
}

/// Index of the last occurrence of `ch` not preceded by an odd run of
/// backslashes, or npos.
[[nodiscard]] inline std::size_t find_last_unescaped_char(
    std::string_view str, char ch) noexcept {
    if (str.empty()) return std::string_view::npos;
    std::size_t i = str.size();
    do {
        --i;
        if (str[i] != ch) continue;
        int backslash_count = 0;
        std::size_t j = i;
        while (j > 0 && str[j - 1] == '\\') {
            ++backslash_count;
            --j;
        }
        if (backslash_count % 2 == 0) return i;  // unescaped
    } while (i > 0);
    return std::string_view::npos;
}

}  // namespace detail

/// Parse a raw permission rule string ("Tool" or "Tool(content)").
///
/// Only the presence of content matters for deny filtering: content-bearing
/// rules never strip a whole tool. The content string itself is therefore not
/// returned. Empty content ("Bash()") and wildcard content ("Bash(*)") are
/// treated as bare tool-name rules.
[[nodiscard]] inline ParsedDenyRule parse_deny_rule(
    std::string_view rule_string) {
    const auto open = detail::find_first_unescaped_char(rule_string, '(');
    if (open == std::string_view::npos) {
        return {detail::normalize_legacy_tool_name(rule_string), false};
    }

    const auto close = detail::find_last_unescaped_char(rule_string, ')');
    if (close == std::string_view::npos || close <= open) {
        return {detail::normalize_legacy_tool_name(rule_string), false};
    }

    // Closing paren must end string
    if (close != rule_string.size() - 1) {
        return {detail::normalize_legacy_tool_name(rule_string), false};
    }

    const std::string_view tool_name = rule_string.substr(0, open);
    // Missing tool name is malformed
    if (tool_name.empty()) {
        return {detail::normalize_legacy_tool_name(rule_string), false};
    }

    const std::string_view raw_content =
        rule_string.substr(open + 1, close - open - 1);
    // '' or '*' is tool-wide
    if (raw_content.empty() || raw_content == "*") {
        return {detail::normalize_legacy_tool_name(tool_name), false};
    }

    // Content is not unescaped — deny matching only needs to know content
    // exists.
    return {detail::normalize_legacy_tool_name(tool_name), true};
}

// ============================================================
// Tool view + name used for permission checks
// ============================================================

/// Minimal description of a tool at the filtering point. Built-ins carry only
/// `name`; dynamic (MCP) defs additionally carry the RAW server/tool names so
/// the qualified check name can be synthesized even though the model-facing
/// definition name is the short unprefixed form in the CPP port.
struct DenyToolView {
    std::string name;
    std::optional<std::string> mcp_server;
    std::optional<std::string> mcp_tool;
};

/// Build the name used for permission rule matching.
[[nodiscard]] inline std::string permission_check_name(
    const DenyToolView& tool) {
    if (tool.mcp_server) {
        std::string result = "mcp__";
        result += normalize_name_for_mcp(*tool.mcp_server);
        result += "__";
        result += normalize_name_for_mcp(tool.mcp_tool.value_or(std::string{}));
        return result;
    }
    return tool.name;
}

// ============================================================
// Rule matching
// ============================================================

/// True when a single raw deny rule strips the tool whose qualified
/// check-name is `name_for_check`.
///
/// Semantics:
///   (a) content rules ("Bash(npm install)") never match a whole tool;
///   (b) direct equality between the parsed rule tool name and the check name;
///   (c) MCP server scope: rule "mcp__server" / "mcp__server__*" matches any
///       "mcp__server__tool" by normalized server-name equality.
/// An exact-tool MCP rule matches only via the direct equality in (b).
[[nodiscard]] inline bool deny_rule_matches(
    std::string_view rule_string,
    std::string_view name_for_check) {
    const ParsedDenyRule rule = parse_deny_rule(rule_string);

    // Content-bearing rule => no whole-tool match
    if (rule.has_content) return false;

    // Direct tool name match
    if (rule.tool_name == name_for_check) return true;

    // MCP server-level permission
    const auto rule_info = mcp_info_from_string(rule.tool_name);
    const auto tool_info = mcp_info_from_string(name_for_check);
    if (!rule_info || !tool_info) return false;
    const bool server_scoped =
        !rule_info->tool.has_value() || *rule_info->tool == "*";
    return server_scoped && rule_info->server == tool_info->server;
}

/// True when ANY of the raw deny rule strings strips the given tool.
/// An empty rule list matches nothing. Garbage rules simply match nothing;
/// the matcher never throws on malformed input.
[[nodiscard]] inline bool is_tool_denied(
    std::span<const std::string> deny_rule_strings,
    const DenyToolView& tool) {
    if (deny_rule_strings.empty()) return false;
    const std::string name_for_check = permission_check_name(tool);
    for (const std::string& rule_string : deny_rule_strings) {
        if (deny_rule_matches(rule_string, name_for_check)) return true;
    }
    return false;
}

}  // namespace loom::utils::tool_deny_rules
