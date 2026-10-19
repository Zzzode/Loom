# Global and project instructions

Loom loads user instructions independently of the working directory. Put personal
rules in `~/.loom/LOOM.md`. Use the exact uppercase filename: `Loom.md` can appear
to work on a case-insensitive macOS volume but is not a portable spelling.
These instructions supplement the built-in system prompt; they are guidance for
the model, not client-enforced permissions.

## Discovery

The user-level lookup checks for an instruction file in each configuration
directory, rather than selecting a directory just because it exists:

1. If `LOOM_CONFIG_DIR` is nonempty, use only that directory. It is an explicit
   isolated profile; no parent-directory or home-directory fallback applies.
2. Otherwise check `~/.loom`, then `~/.agents`.
3. Within each directory, choose the first existing file named `LOOM.md`,
   `AGENTS.md` or `CLAUDE.md`, in that order. At most one user file is loaded.

An empty `~/.loom` directory does not hide an instruction file in `~/.agents`.
Loom never looks in the ancestors of a configuration directory for user rules.
When no file exists, the default creation location is the writable config
directory's `LOOM.md`; reading alone does not create it.

Project instructions are separate: from the working directory, Loom walks upward
and loads the nearest `LOOM.md`, `AGENTS.md` or `CLAUDE.md`, applying the same name
order within a directory. It currently loads one nearest project file, rather
than concatenating every ancestor. A project file and user file resolving to the
same actual file are loaded once.

Main sessions include the selected user file in a `UserMemory` context. Native
agents include it when constructing a new default or definition-based system
prompt. Explicit complete parent-system-prompt overrides, including implicit
forks, keep their inherited prompt rather than appending a second copy. This
preserves the complete-prompt override contract.

## Example

Create `~/.loom/LOOM.md` with concise reusable rules, for example:

```markdown
# Working preferences

- Use English for source code and code comments.
- Explain changes in Chinese.
- Preserve unrelated uncommitted work.
- Run the project's relevant checks before claiming completion.
```

A new session reads the file regardless of whether it starts in a repository,
a subdirectory, or an unrelated directory such as `/tmp`. Existing sessions do
not automatically refresh an already assembled system prompt. Keep project-only
build commands and architecture rules in the project's instruction file.

Each machine reads its own current user's home directory. Cross-machine reuse
requires copying, dotfile management or another synchronization mechanism;
Loom does not synchronize personal instruction files automatically.

## Comparison with other agents

Official documentation checked on 2026-10-19:

| Agent | User-level file | Project behavior |
|---|---|---|
| [Claude Code](https://code.claude.com/docs/en/memory) | `~/.claude/CLAUDE.md` | Loads ancestor project instructions at launch; deeper files are discovered as work reaches them. |
| [Codex](https://learn.chatgpt.com/docs/agent-configuration/agents-md) | `$CODEX_HOME/AGENTS.override.md`, otherwise `AGENTS.md`; default home is `~/.codex` | Builds a root-to-working-directory chain, one selected instruction file per directory, with a configured size cap. |
| [Gemini CLI](https://geminicli.com/docs/cli/gemini-md/) | `~/.gemini/GEMINI.md` | Combines global, workspace and just-in-time directory context. |
| Loom | `~/.loom/LOOM.md`, with the compatibility fallback described above | Loads the nearest project file plus the separate global user file. |

The shared pattern is a stable user-level instruction file plus more specific
project guidance. Keep global rules short and broadly applicable. Project
hierarchy details differ between tools; do not assume their discovery algorithms
are interchangeable. Global instruction files are distinct from auto-generated
memory indexes, slash-command prompt templates and a replacement base system
prompt.
