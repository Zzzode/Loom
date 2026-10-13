/// Grep tool backed by ripgrep. Process execution stays local to this tool.
export module loom.tools.grep;

import std;
import loom.utils.error;
import loom.tools.tool;

export namespace loom::tools::grep {

class GrepTool {
public:
    static constexpr std::string_view kName = "Grep";
    static constexpr std::string_view kDescription =
        "Search file contents with ripgrep regex syntax. Respects ignore files and skips "
        "binary/hidden files during directory searches. Requires rg on PATH.";

    [[nodiscard]] static loom::core::ToolDefinition definition();
    [[nodiscard]] bool check_permission(const loom::core::ToolInput&) const { return true; }
    [[nodiscard]] loom::utils::Result<loom::core::ToolResult> execute(const loom::core::ToolInput& input);
};

} // namespace loom::tools::grep

export namespace loom::tools {
    using loom::tools::grep::GrepTool;
    [[nodiscard]] std::unique_ptr<loom::core::ITool> make_grep_tool();
}
