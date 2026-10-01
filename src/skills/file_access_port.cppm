/// @file file_access_port.cppm
/// @brief File-access hook port — skills-owned callback leaf.
/// The file tools depend on this rank-5 contract instead of the concrete
/// loom.skills.skill module, breaking the tools<->skills module cycle.
module;

export module loom.skills.file_access.port;

import std;

export namespace loom::skills {

// ============================================================
// File Access Hook - cross-module skill discovery trigger
// ============================================================
//
// The file tools (loom_tools) need to trigger skill discovery after
// read/write/edit operations, but loom_tools cannot depend on loom_skills
// (which owns load_skills_dir / SkillRegistry) due to a circular
// dependency: loom_skills already links loom_tools.
//
// Solution: a lightweight callback registered in loom_skills_core.
// The loom_skills module sets the hook at startup; file tools call
// notify_file_access() which invokes the hook if set.

/// Hook type: called after a file is read/written/edited.
/// @param file_path  Absolute path of the accessed file
/// @param cwd        Current working directory (for skill path resolution)
using FileAccessHook = std::function<void(const std::filesystem::path& file_path,
                                           const std::filesystem::path& cwd)>;

namespace detail {
/// Internal storage for the hook (function-local static to avoid
/// header-order initialization issues).
[[nodiscard]] inline FileAccessHook& file_access_hook() {
    static FileAccessHook instance;
    return instance;
}
} // namespace detail

/// Register the file-access hook. Called by loom_skills at init time.
/// Pass nullptr to unregister.
inline void set_file_access_hook(FileAccessHook hook) {
    detail::file_access_hook() = std::move(hook);
}

/// Notify the file-access hook (if registered). Called by file tools
/// after successful read/write/edit operations.
/// TS REF: src/tools/FileReadTool/FileReadTool.ts L579-590
///          src/tools/FileWriteTool/FileWriteTool.ts L234-245
///          src/tools/FileEditTool/FileEditTool.ts L408-422
inline void notify_file_access(
    const std::filesystem::path& file_path,
    const std::filesystem::path& cwd)
{
    auto& hook = detail::file_access_hook();
    if (hook) {
        try {
            hook(file_path, cwd);
        } catch (...) {
            // Skill discovery is best-effort; never fail the file operation.
        }
    }
}

} // namespace loom::skills
