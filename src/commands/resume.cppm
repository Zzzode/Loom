/// @file resume.cppm
/// @brief ResumeCommand implementing the /resume slash command.
/// List recent sessions, resume by ID, resume last session, show session preview.
module;

#include <cstdint>
#include <fstream>

export module loom.commands.resume;

import std;

import loom.types.types;
import loom.commands.command;
import loom.session.storage;
import loom.serdes.json;
export namespace loom::commands {

using namespace loom::core;

/// Summary of a resumable session
struct SessionSummary {
    SessionId id;
    std::string title;
    std::uint32_t message_count;
    std::chrono::system_clock::time_point last_active;
    std::string model;
};

/// ResumeCommand implements the /resume slash command.
/// Allows resuming previous conversation sessions.
class ResumeCommand {
public:
    [[nodiscard]] static CommandDefinition definition() {
        return CommandDefinition{
            .name = "resume",
            .description = "Resume a previous conversation session",
            .args = {
                CommandArg{.name = "session_id", .description = "Session ID to resume, or 'last'",
                           .type = ArgType::Text, .required = false},
            },
            .category = "session",
            .aliases = {"r"},
            .hidden = false,
            .argument_hint = "<session_id|last>",
        };
    }

    [[nodiscard]] VoidResult validate(const CommandContext& /*ctx*/) {
        return {};
    }

    [[nodiscard]] Result<CommandResult> execute(const CommandContext& ctx) {
        // Refresh the session list from disk on every invocation.  The
        // set_recent_sessions() setter exists for tests/external population,
        // but the app never calls it — without this load the list is always
        // empty and /resume reports "No recent sessions found."
        load_sessions();

        if (ctx.args.empty()) {
            // Signal the app to open the interactive session picker dialog.
            // The app handles "UI:resume" by loading sessions from disk and
            // pushing a SessionPickerPayload onto the dialog queue.
            return CommandResult{true, "Opening session picker...", "UI:resume",
                                 CommandStatus::Succeeded};
        }

        auto arg = std::string(ctx.args[0]);

        if (arg == "last") {
            return resume_last();
        }
        if (arg == "list") {
            return CommandResult::success(format_recent_sessions());
        }

        // Treat as session ID
        return resume_by_id(arg);
    }

    [[nodiscard]] std::vector<std::string> complete(std::string_view partial) {
        std::vector<std::string> suggestions;
        if (std::string_view("last").starts_with(partial)) {
            suggestions.emplace_back("last");
        }
        if (std::string_view("list").starts_with(partial)) {
            suggestions.emplace_back("list");
        }
        // Suggest known session IDs
        for (const auto& s : recent_sessions_) {
            if (s.id.str().starts_with(partial)) {
                suggestions.push_back(s.id.str());
            }
        }
        return suggestions;
    }

    /// Set the list of recent sessions (populated by the session manager)
    void set_recent_sessions(std::vector<SessionSummary> sessions) {
        recent_sessions_ = std::move(sessions);
    }

private:
    /// Load recent sessions from disk (~/.loom/sessions) into recent_sessions_.
    /// Uses the same default directory as SessionStorage so the list matches
    /// what the app actually persists.  list_recent_sessions sorts by
    /// last_active descending (newest first), matching recent_sessions_'
    /// expected order for resume_last().  Sessions with zero messages are
    /// filtered out — they are empty shells created on startup that the user
    /// never interacted with.  The message count is read from the actual
    /// file on disk (messages.jsonl lines or messages.json array size)
    /// rather than metadata.json's message_count, which is 0 for sessions
    /// that use messages.jsonl — the engine appends to the file but doesn't
    /// update the metadata count.
    void load_sessions() {
        const char* home = std::getenv("HOME");
        const auto sessions_dir =
            std::filesystem::path{home ? home : "/tmp"} / ".loom" / "sessions";
        auto metas = loom::session::list_recent_sessions(sessions_dir, 20);
        recent_sessions_.clear();
        recent_sessions_.reserve(metas.size());
        for (auto& m : metas) {
            int actual_count = count_messages_on_disk(sessions_dir, m.session_id);
            if (actual_count == 0) continue;
            recent_sessions_.push_back(SessionSummary{
                .id = SessionId{.value = m.session_id},
                .title = m.title.value_or("Session"),
                .message_count = static_cast<std::uint32_t>(actual_count),
                .last_active = m.last_active,
                .model = m.model,
            });
        }
    }

    std::vector<SessionSummary> recent_sessions_;

    [[nodiscard]] std::string format_recent_sessions() const {
        if (recent_sessions_.empty()) {
            return "No recent sessions found.\nStart a new conversation to create a session.";
        }
        const char* home = std::getenv("HOME");
        const auto sessions_dir =
            std::filesystem::path{home ? home : "/tmp"} / ".loom" / "sessions";
        std::string out = "Recent sessions:\n";
        for (const auto& s : recent_sessions_) {
            auto age = format_time_ago(s.last_active);
            const auto display_title = effective_title(s, sessions_dir);
            out += std::format("  {} — {} ({} msgs, {})\n",
                short_id(s.id.str()), display_title, s.message_count, age);
        }
        out += "\nUse /resume <id> or /resume last";
        return out;
    }

    [[nodiscard]] Result<CommandResult> resume_last() const {
        if (recent_sessions_.empty()) {
            return std::unexpected(Error::make(ErrorCode::SessionNotFound,
                "No previous sessions to resume."));
        }
        const auto& last = recent_sessions_.front();
        // Signal the app to load this session's messages into the transcript.
        return CommandResult{true,
            std::format("Resuming session: {} ({})", last.id.str().substr(0, 8), last.title),
            "UI:resume:" + last.id.str(),
            CommandStatus::Succeeded};
    }

    [[nodiscard]] Result<CommandResult> resume_by_id(const std::string& id) const {
        auto it = std::ranges::find_if(recent_sessions_,
            [&](const auto& s) { return s.id.str().starts_with(id); });
        if (it == recent_sessions_.end()) {
            return std::unexpected(Error::make(ErrorCode::SessionNotFound,
                std::format("Session '{}' not found. Use /resume to list available sessions.", id)));
        }
        // Signal the app to load this session's messages into the transcript.
        return CommandResult{true,
            std::format("Resuming session: {} ({})", it->id.str().substr(0, 8), it->title),
            "UI:resume:" + it->id.str(),
            CommandStatus::Succeeded};
    }

    /// Extract a short, distinguishing ID from a session ID.
    /// UUID format (e.g. "82702bdd-679b-…") → first 8 chars.
    /// session_<ts>_<hex> format → last 8 chars (the hex suffix),
    /// because substr(0,8) of "session_…" is just "session_" for all.
    [[nodiscard]] static std::string short_id(const std::string& id) {
        if (id.starts_with("session_")) {
            auto pos = id.rfind('_');
            if (pos != std::string::npos && pos + 1 < id.size())
                return id.substr(pos + 1, 8);
        }
        return id.substr(0, 8);
    }

    /// Return a useful title for the session.  When the stored title is
    /// empty or the generic "Session", fall back to the first user
    /// message in messages.jsonl (truncated) so the user can actually
    /// tell sessions apart.  Handles both content formats the engine
    /// writes: plain string (single TextBlock) and content-block array.
    /// Also falls back to the legacy messages.json format which uses a
    /// top-level "text" field instead of "content".
    [[nodiscard]] std::string effective_title(
        const SessionSummary& s,
        const std::filesystem::path& sessions_dir) const {
        if (!s.title.empty() && s.title != "Session") return s.title;

        // ── Current format: messages.jsonl (one JSON object per line) ──
        auto docs = loom::session::load_messages(sessions_dir, s.id.str());
        for (std::size_t i = 0; i < docs.size(); ++i) {
            auto root = docs[i].root();
            if (!root.valid() || !root.is_obj()) continue;
            auto role_val = root.get("role");
            if (!role_val.is_str() || role_val.as_str() != std::string_view("user"))
                continue;
            auto content_val = root.get("content");
            if (!content_val.valid()) continue;

            std::string text;
            if (content_val.is_str()) {
                text = std::string(content_val.as_str());
            } else if (content_val.is_arr()) {
                for (std::size_t j = 0; j < content_val.size(); ++j) {
                    auto block = content_val.at(j);
                    if (!block.is_obj()) continue;
                    auto type_val = block.get("type");
                    if (!type_val.is_str() ||
                        type_val.as_str() != std::string_view("text"))
                        continue;
                    auto text_val = block.get("text");
                    if (!text_val.is_str()) continue;
                    text = std::string(text_val.as_str());
                    break;
                }
            }

            if (!text.empty()) {
                for (auto& c : text) if (c == '\n') c = ' ';
                if (text.size() > 60) text = text.substr(0, 60) + "…";
                return text;
            }
        }

        // ── Legacy format: messages.json (JSON array with "text" field) ──
        auto json_path = sessions_dir / s.id.str() / "messages.json";
        if (std::filesystem::exists(json_path)) {
            auto doc = loom::utils::json::parse_file(json_path);
            if (doc) {
                auto root = doc->root();
                if (root.is_arr()) {
                    for (std::size_t i = 0; i < root.size(); ++i) {
                        auto msg = root.at(i);
                        if (!msg.is_obj()) continue;
                        auto role_val = msg.get("role");
                        if (!role_val.is_str() ||
                            role_val.as_str() != std::string_view("user"))
                            continue;
                        auto text_val = msg.get("text");
                        if (!text_val.is_str()) continue;
                        auto text = std::string(text_val.as_str());
                        if (!text.empty()) {
                            for (auto& c : text) if (c == '\n') c = ' ';
                            if (text.size() > 60) text = text.substr(0, 60) + "…";
                            return text;
                        }
                    }
                }
            }
        }

        return "Session";
    }

    /// Count messages in a session by inspecting the actual file on disk.
    /// Prefers messages.jsonl (current format); falls back to messages.json
    /// (legacy JSON-array format).  Returns 0 if neither file exists.
    [[nodiscard]] static int count_messages_on_disk(
        const std::filesystem::path& sessions_dir,
        const std::string& session_id)
    {
        auto dir = sessions_dir / session_id;
        auto jsonl_path = dir / "messages.jsonl";
        if (std::filesystem::exists(jsonl_path)) {
            std::ifstream ifs(jsonl_path);
            std::string line;
            int count = 0;
            while (std::getline(ifs, line)) {
                if (!line.empty()) ++count;
            }
            return count;
        }
        auto json_path = dir / "messages.json";
        if (std::filesystem::exists(json_path)) {
            auto doc = loom::utils::json::parse_file(json_path);
            if (doc) {
                auto root = doc->root();
                if (root.is_arr()) return static_cast<int>(root.size());
            }
        }
        return 0;
    }

    [[nodiscard]] static std::string format_time_ago(std::chrono::system_clock::time_point tp) {
        auto elapsed = std::chrono::system_clock::now() - tp;
        auto hours = std::chrono::duration_cast<std::chrono::hours>(elapsed).count();
        if (hours < 1) return "just now";
        if (hours < 24) return std::format("{}h ago", hours);
        return std::format("{}d ago", hours / 24);
    }
};

} // namespace loom::commands
