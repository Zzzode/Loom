// Implementation unit for loom.tools.agent_worktree (RFC-0001 B14). The body
// moved verbatim from agent_sub_utils_hooks.cpp; the only spelling change is
// shell_quote -> loom::tools::runtime_shared_utils::shell_quote (a byte-identical
// single-quote escaper): the agent.utils shell_quote stays in the hooks
// impl unit for run_agent_command_hook, and importing agent.utils from this
// leaf would point back into the subtree B15 lifts into loom.orchestration.
module;

module loom.tools.agent_worktree;

import std;

import loom.tools.agent_runtime;
import loom.tools.runtime_shared_utils;
import loom.scm.git.git;

namespace loom::tools::agent {

namespace fs = std::filesystem;

[[nodiscard]] AgentWorktreeCleanupResult cleanup_agent_worktree(std::string_view agent_id) {
    auto record = loom::tools::agent_runtime::native_agent_store().get(agent_id);
    if (!record || !record->worktree_path || !record->worktree_git_root || !record->worktree_branch) {
        return {};
    }

    AgentWorktreeCleanupResult result{
        .attempted = true,
        .message = "worktree cleanup inspected",
    };

    const auto worktree_path = fs::path{*record->worktree_path};
    const auto git_root = fs::path{*record->worktree_git_root};
    const auto branch = *record->worktree_branch;

    std::error_code ec;
    if (!fs::exists(worktree_path, ec)) {
        loom::tools::agent_runtime::native_agent_store().mark_worktree_cleaned(agent_id);
        result.removed = true;
        result.message = "worktree was already absent and metadata was cleaned";
        return result;
    }

    auto status = loom::utils::git::run_git_command("status --porcelain", worktree_path);
    if (!status.success) {
        result.changed = true;
        result.message = "worktree status could not be inspected; preserving worktree";
        loom::tools::agent_runtime::native_agent_store().append_transcript(
            agent_id,
            "system: retained worktree at " + worktree_path.string() + " because status inspection failed");
        return result;
    }
    if (!status.output.empty()) {
        result.changed = true;
        result.message = "worktree has uncommitted changes and was preserved";
        loom::tools::agent_runtime::native_agent_store().append_transcript(
            agent_id,
            "system: retained worktree with uncommitted changes at " + worktree_path.string());
        return result;
    }

    if (record->worktree_base_commit && !record->worktree_base_commit->empty()) {
        auto head = loom::utils::git::run_git_command("rev-parse HEAD", worktree_path);
        if (!head.success) {
            result.changed = true;
            result.message = "worktree HEAD could not be inspected; preserving worktree";
            loom::tools::agent_runtime::native_agent_store().append_transcript(
                agent_id,
                "system: retained worktree at " + worktree_path.string() + " because HEAD inspection failed");
            return result;
        }
        if (head.output != *record->worktree_base_commit) {
            result.changed = true;
            result.message = "worktree branch contains commits and was preserved";
            loom::tools::agent_runtime::native_agent_store().append_transcript(
                agent_id,
                "system: retained worktree branch " + branch + " at " + worktree_path.string());
            return result;
        }
    }

    auto removed = loom::utils::git::run_git_command(
        "worktree remove --force " + loom::tools::runtime_shared_utils::shell_quote(worktree_path.string()),
        git_root);
    if (!removed.success) {
        result.changed = true;
        result.message = "worktree removal failed; preserving metadata";
        loom::tools::agent_runtime::native_agent_store().append_transcript(
            agent_id,
            "system: failed to remove worktree at " + worktree_path.string() + ": " + removed.output);
        return result;
    }

    (void)loom::utils::git::run_git_command("branch -D " + loom::tools::runtime_shared_utils::shell_quote(branch), git_root);
    loom::tools::agent_runtime::native_agent_store().mark_worktree_cleaned(agent_id);
    loom::tools::agent_runtime::native_agent_store().append_transcript(
        agent_id,
        "system: cleaned worktree " + worktree_path.string() + " and branch " + branch);
    result.removed = true;
    result.message = "worktree removed";
    return result;
}

} // namespace loom::tools::agent
