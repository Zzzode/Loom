// Serialized owning TeamStore operations and independent value snapshots.
module;

module loom.tools.team;

import std;

namespace loom::tools {

auto TeamStore::create(std::string id, std::string team_name, std::vector<TeamMember> members)
    -> std::expected<Team, TeamError> {
    std::scoped_lock lock(mutex_);
    if (id.empty()) return std::unexpected(TeamError::IdEmpty);
    if (team_name.empty()) return std::unexpected(TeamError::NameEmpty);
    if (teams_.contains(id)) return std::unexpected(TeamError::TeamAlreadyExists);
    if (teams_.size() >= kMaxTeams) return std::unexpected(TeamError::TooManyTeams);
    if (members.size() > kMaxMembersPerTeam) return std::unexpected(TeamError::TooManyMembers);

    Team team{
        .id = id,
        .name = std::move(team_name),
        .members = std::move(members),
        .task_list = {},
        .created_at = std::chrono::steady_clock::now(),
    };
    auto [it, _] = teams_.emplace(std::move(id), std::move(team));
    (void)persist_team_record(it->second);
    return it->second;
}

auto TeamStore::get_locked(const std::string& id) -> std::expected<Team*, TeamError> {
    auto it = teams_.find(id);
    if (it == teams_.end()) {
        auto loaded = load_team_record(id);
        if (!loaded) return std::unexpected(TeamError::TeamNotFound);
        auto [inserted, _] = teams_.emplace(loaded->id, std::move(*loaded));
        it = inserted;
    }
    return &it->second;
}

auto TeamStore::get(const std::string& id) -> std::expected<Team, TeamError> {
    std::scoped_lock lock(mutex_);
    auto found = get_locked(id);
    if (!found) return std::unexpected(found.error());
    return **found;
}

auto TeamStore::get_by_id_or_name_locked(const std::string& key)
    -> std::expected<Team*, TeamError> {
    if (auto by_id = get_locked(key)) return by_id;

    for (auto& [_, team] : teams_) {
        if (team.name == key) return &team;
    }

    std::error_code ec;
    if (!fs::exists(team_runtime_dir(), ec)) return std::unexpected(TeamError::TeamNotFound);
    for (const auto& entry : fs::directory_iterator(team_runtime_dir(), ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".json") continue;
        auto loaded = load_team_record_from_path(entry.path());
        if (!loaded) continue;
        auto id = loaded->id;
        auto it = teams_.find(id);
        if (it == teams_.end()) {
            auto [inserted, _] = teams_.emplace(std::move(id), std::move(*loaded));
            it = inserted;
        } else {
            it->second = std::move(*loaded);
        }
        if (it->second.id == key || it->second.name == key) return &it->second;
    }
    return std::unexpected(TeamError::TeamNotFound);
}

auto TeamStore::get_by_id_or_name(const std::string& key) -> std::expected<Team, TeamError> {
    std::scoped_lock lock(mutex_);
    auto found = get_by_id_or_name_locked(key);
    if (!found) return std::unexpected(found.error());
    return **found;
}

auto TeamStore::add_member(const std::string& team_key, TeamMember member)
    -> std::expected<void, TeamError> {
    std::scoped_lock lock(mutex_);
    auto team = get_by_id_or_name_locked(team_key);
    if (!team) return std::unexpected(team.error());
    if (member.agent_id.empty()) return std::unexpected(TeamError::MemberNotFound);

    auto existing = std::ranges::find_if((*team)->members, [&](const auto& candidate) {
        return candidate.agent_id == member.agent_id;
    });
    if (existing != (*team)->members.end()) {
        existing->role = member.role;
        existing->status = member.status;
        existing->current_task = std::move(member.current_task);
        existing->last_result = std::move(member.last_result);
        (void)persist_team_record(**team);
        return {};
    }

    if ((*team)->members.size() >= kMaxMembersPerTeam) {
        return std::unexpected(TeamError::TooManyMembers);
    }
    (*team)->members.push_back(std::move(member));
    (void)persist_team_record(**team);
    return {};
}

auto TeamStore::remove(const std::string& id) -> std::expected<void, TeamError> {
    std::scoped_lock lock(mutex_);
    if (!teams_.contains(id) && !fs::exists(team_record_path(id))) {
        return std::unexpected(TeamError::TeamNotFound);
    }
    teams_.erase(id);
    std::error_code ec;
    fs::remove(team_record_path(id), ec);
    return {};
}

auto TeamStore::add_task(const std::string& team_id, SharedTaskItem task)
    -> std::expected<void, TeamError> {
    std::scoped_lock lock(mutex_);
    auto team = get_locked(team_id);
    if (!team) return std::unexpected(team.error());
    if ((*team)->task_list.size() >= kMaxTasksPerTeam) {
        return std::unexpected(TeamError::TaskListFull);
    }
    (*team)->task_list.push_back(std::move(task));
    (void)persist_team_record(**team);
    return {};
}

auto TeamStore::assign_task(const std::string& team_id, const std::string& task_id,
                            const std::string& agent_id) -> std::expected<void, TeamError> {
    std::scoped_lock lock(mutex_);
    auto team = get_locked(team_id);
    if (!team) return std::unexpected(team.error());

    // Find the task
    auto task_it =
        std::ranges::find_if((*team)->task_list, [&](const auto& t) { return t.id == task_id; });
    if (task_it == (*team)->task_list.end()) {
        return std::unexpected(TeamError::InvalidOperation);
    }

    // Find the member
    auto member_it = std::ranges::find_if((*team)->members,
                                          [&](const auto& m) { return m.agent_id == agent_id; });
    if (member_it == (*team)->members.end()) {
        return std::unexpected(TeamError::MemberNotFound);
    }

    task_it->assigned_to = agent_id;
    member_it->current_task = task_id;
    member_it->status = MemberStatus::Working;
    (void)persist_team_record(**team);
    return {};
}

auto TeamStore::complete_task(const std::string& team_id, const std::string& task_id,
                              std::string result) -> std::expected<void, TeamError> {
    std::scoped_lock lock(mutex_);
    auto team = get_locked(team_id);
    if (!team) return std::unexpected(team.error());

    auto task_it =
        std::ranges::find_if((*team)->task_list, [&](const auto& t) { return t.id == task_id; });
    if (task_it == (*team)->task_list.end()) {
        return std::unexpected(TeamError::InvalidOperation);
    }

    task_it->completed = true;
    task_it->result = std::move(result);

    // Update member status
    if (task_it->assigned_to) {
        auto member_it = std::ranges::find_if(
            (*team)->members, [&](const auto& m) { return m.agent_id == *task_it->assigned_to; });
        if (member_it != (*team)->members.end()) {
            member_it->status = MemberStatus::Done;
            member_it->last_result = task_it->result;
            member_it->current_task = std::nullopt;
        }
    }
    (void)persist_team_record(**team);
    return {};
}

auto TeamStore::update_member_status(const std::string& team_key, const std::string& agent_id,
                                     MemberStatus status, std::optional<std::string> result)
    -> std::expected<void, TeamError> {
    std::scoped_lock lock(mutex_);
    auto team = get_by_id_or_name_locked(team_key);
    if (!team) return std::unexpected(team.error());

    auto member_it = std::ranges::find_if(
        (*team)->members, [&](const auto& member) { return member.agent_id == agent_id; });
    if (member_it == (*team)->members.end()) {
        return std::unexpected(TeamError::MemberNotFound);
    }

    member_it->status = status;
    if (status != MemberStatus::Working) {
        member_it->current_task = std::nullopt;
    }
    if (result) {
        member_it->last_result = std::move(result);
    }
    (void)persist_team_record(**team);
    return {};
}

void TeamStore::clear_for_testing() {
    std::scoped_lock lock(mutex_);
    teams_.clear();
}

std::expected<Team, TeamError> TeamCreateTool::execute(std::string id, std::string team_name,
                                                       std::vector<TeamMember> members) {
    return global_team_store().create(std::move(id), std::move(team_name), std::move(members));
}

}  // namespace loom::tools
