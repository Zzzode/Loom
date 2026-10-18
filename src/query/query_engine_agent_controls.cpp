// Host-owned agent controls reuse the engine's normal policy dispatch.
module;

module loom.query.query_engine;

import std;
import loom.tools.agent_runtime;
import loom.serdes.json;

namespace loom::core {
namespace {
namespace runtime = loom::tools::agent_runtime;
namespace json = loom::utils::json;

std::string host_continue_record_reason(const runtime::NativeAgentRecord& record,
                                       std::string_view session) {
    if (session.empty() || !record.parent_session_id || *record.parent_session_id != session) {
        return "Agent does not belong to this session";
    }
    if (runtime::is_team_agent_record(record)) return "Team agents cannot be continued here";
    if (record.parent_agent_id && !record.parent_agent_id->empty()) {
        return "Nested agents cannot be continued here";
    }
    if (record.status == runtime::NativeAgentStatus::Queued ||
        record.status == runtime::NativeAgentStatus::Running) return "Agent has not finished";
    if (record.sidechain_entries.empty() && record.transcript.empty()) return "No retained context is available";
    return {};
}
}

std::string QueryEngine::agent_continue_unavailable_reason(std::string_view id) const {
    auto& store = runtime::native_agent_store();
    auto record = store.get(id);
    if (!record) return "Agent not found";
    if (auto reason = host_continue_record_reason(*record, session_id_.str()); !reason.empty()) return reason;
    if (auto reason = store.resume_unavailable_reason(id); !reason.empty()) return reason;
    const auto definitions = runtime::get_all_agent_definitions(
        record->cwd ? std::optional<std::filesystem::path>{*record->cwd} : std::nullopt);
    if (!runtime::resolve_agent_type(record->agent_type, definitions)) return "Agent definition is no longer available";
    return {};
}

std::string QueryEngine::agent_stop_unavailable_reason(std::string_view id) const {
    return runtime::native_agent_store().owned_stop_unavailable_reason(
        id, session_id_.str(), tool_registry_->background_stop_token());
}

VoidResult QueryEngine::stop_agent_instance(std::string_view id) {
    auto stopped = runtime::native_agent_store().request_owned_cancel(
        id, session_id_.str(), tool_registry_->background_stop_token());
    if (!stopped) return std::unexpected(Error::make(ErrorCode::InvalidRequest, stopped.error()));
    return {};
}

Result<std::string> QueryEngine::continue_agent_instance(std::string_view id, std::string_view prompt) {
    if (prompt.empty()) return std::unexpected(Error::make(ErrorCode::InvalidRequest, "Continuation message is empty"));
    if (auto reason = agent_continue_unavailable_reason(id); !reason.empty()) {
        return std::unexpected(Error::make(ErrorCode::InvalidRequest, std::move(reason)));
    }
    aborted_.store(false);
    set_abort_callback(nullptr);
    const auto record = runtime::native_agent_store().get(id);
    if (!record) return std::unexpected(Error::make(ErrorCode::InvalidRequest, "Agent not found"));
    const auto owner_session = session_id_.str();
    const auto agent_id = std::string(id);
    const auto agent_type = record->agent_type;
    const auto cwd = record->cwd;
    json::JsonMutDoc doc;
    auto object = doc.object();
    object.add("agent_id", doc.string(agent_id));
    object.add("subagent_type", doc.string(agent_type));
    object.add("prompt", doc.string(prompt));
    object.add("resume_existing", doc.boolean(true));
    object.add("run_in_background", doc.boolean(true));
    if (cwd) object.add("cwd", doc.string(*cwd));
    doc.set_root(object);
    ToolUseBlock tool;
    tool.id.value = generate_id();
    tool.name = "Agent";
    tool.input_json = doc.to_string();
    const auto validate = [this, agent_id, agent_type, owner_session, cwd](std::string_view payload) -> std::string {
        if (session_id_.str() != owner_session) return "Agent session changed before continuation";
        if (auto reason = agent_continue_unavailable_reason(agent_id); !reason.empty()) return reason;
        auto parsed = json::parse(payload);
        if (!parsed || !parsed->root().is_obj()) return "Invalid host continuation payload";
        auto root = parsed->root();
        bool invalid = false;
        root.iter_obj([&](json::JsonVal key, json::JsonVal) {
            const auto name = key.as_str();
            if (name != "agent_id" && name != "subagent_type" && name != "prompt" &&
                name != "resume_existing" && name != "run_in_background" && name != "cwd") invalid = true;
        });
        const auto matches = [&](std::string_view key, std::string_view value) {
            auto field = root.get(key);
            return field.is_str() && field.as_str() == value;
        };
        if (invalid || !matches("agent_id", agent_id) || !matches("subagent_type", agent_type) ||
            !root.get("prompt").is_str() || root.get("prompt").as_str().empty() ||
            !root.get("resume_existing").is_bool() || !root.get("resume_existing").as_bool() ||
            !root.get("run_in_background").is_bool() || !root.get("run_in_background").as_bool() ||
            (cwd ? !matches("cwd", *cwd) : root.get("cwd").valid())) {
            return "Hook cannot redirect a host agent continuation";
        }
        const auto current = runtime::native_agent_store().get(agent_id);
        if (!current || current->agent_type != agent_type) return "Agent identity changed before continuation";
        return {};
    };
    const auto result = execute_single_tool(tool, QueryOptions{}, validate,
        HostAgentResumeConstraint{agent_id, agent_type, owner_session});
    if (result.is_error) {
        std::string message;
        for (const auto& block : result.content) {
            if (const auto* text = std::get_if<TextBlock>(&block)) message += text->text;
        }
        return std::unexpected(Error::make(ErrorCode::InvalidRequest, std::move(message)));
    }
    return agent_id;
}

} // namespace loom::core
