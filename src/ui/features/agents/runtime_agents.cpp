module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>

module loom.ui.features.agents.runtime_agents;

import std;
import loom.ui.foundation.theme_provider;
import loom.ui.foundation.design_figures;

namespace loom::ui::agents::runtime {

using namespace ftxui;

bool is_active(Status status) noexcept {
    return status == Status::Queued || status == Status::Running;
}

std::string_view status_name(Status status) noexcept {
    switch (status) {
        case Status::Queued: return "queued";
        case Status::Running: return "running";
        case Status::Completed: return "completed";
        case Status::Failed: return "failed";
        case Status::Cancelled: return "cancelled";
    }
    return "queued";
}

namespace {

std::string clipped_line(std::string_view raw, int columns) {
    if (columns <= 0) return {};
    std::string clean;
    clean.reserve(raw.size());
    for (const unsigned char character : raw) {
        clean += character < 0x20 || character == 0x7f ? ' ' : static_cast<char>(character);
    }
    if (string_width(clean) <= columns) return clean;
    std::string clipped;
    int used = 0;
    for (const auto& glyph : Utf8ToGlyphs(clean)) {
        const int width = string_width(glyph);
        if (used + width > columns - 1) break;
        clipped += glyph;
        used += width;
    }
    return clipped + std::string(design::figures::figures().ellipsis);
}

Element summary_row(std::string_view label, int columns) {
    return text(clipped_line(label, columns))
        | color(design::theme::current_theme().palette->muted) | bold
        | size(WIDTH, EQUAL, std::max(0, columns));
}

std::string task_summary(const std::vector<todos::TodoDisplayItem>& items, bool collapsed) {
    const auto done = std::ranges::count_if(items, [](const auto& item) {
        return item.status == todos::TodoDisplayItem::Status::Completed;
    });
    const auto active = std::ranges::count_if(items, [](const auto& item) {
        return item.status == todos::TodoDisplayItem::Status::InProgress;
    });
    if (active != 0)
        return std::format("Tasks {} active · {}/{} done", active, done, items.size());
    return std::format("{}Tasks {}/{} {}",
        collapsed && static_cast<std::size_t>(done) == items.size() ? "✓ " : "", done, items.size(),
        static_cast<std::size_t>(done) == items.size() ? "completed" : "done");
}

std::string subagent_summary(const std::vector<Instance>& instances, int columns) {
    const auto count = [&](Status status) {
        return std::ranges::count_if(instances, [status](const auto& item) { return item.status == status; });
    };
    const auto active = count(Status::Running) + count(Status::Queued);
    const auto failed = count(Status::Failed);
    const auto ended = static_cast<std::ptrdiff_t>(instances.size()) - active;
    std::string label = "Subagents";
    if (count(Status::Running) != 0) label += std::format(" {} running", count(Status::Running));
    if (count(Status::Queued) != 0) label += std::format("{}{} queued", active == count(Status::Queued) ? " " : " · ", count(Status::Queued));
    if (ended != 0) label += std::format("{}{} ended", active == 0 ? " " : " · ", ended);
    if (failed != 0) label += std::format(" · {} failed", failed);
    const std::string hint = " · /subagents";
    if (string_width(label + hint) <= columns) return label + hint;
    if (active != 0) label = std::format("Subagents {} active", active);
    else label = std::format("Subagents {} ended", ended);
    if (failed != 0 && string_width(label + std::format(", {} failed", failed) + hint) <= columns)
        label += std::format(", {} failed", failed);
    return label + hint;
}

std::string team_summary(const std::vector<teams::live::LiveTeammate>& teammates, int columns) {
    const auto running = std::ranges::count_if(teammates, [](const auto& item) { return item.status == "running"; });
    const auto idle = std::ranges::count_if(teammates, [](const auto& item) { return item.status == "idle"; });
    const auto unknown = static_cast<std::ptrdiff_t>(teammates.size()) - running - idle;
    std::string label = "Agent Teams";
    const auto append = [&](auto amount, std::string_view state) {
        if (amount != 0) label += std::format(" · {} agent{} {}", amount, amount == 1 ? "" : "s", state);
    };
    if (running != 0 || unknown != 0) {
        append(running, "running");
        append(unknown, "unknown");
    } else append(idle, "idle");
    if (string_width(label + " · instances") <= columns) return label + " · instances";
    if (running != 0) label = std::format("Agent Teams {} running", running);
    else if (unknown != 0) label = std::format("Agent Teams {} unknown", unknown);
    else label = std::format("Agent Teams {} idle", idle);
    return label + " · instances";
}

std::string fitting_prefix(std::string_view raw, int columns) {
    std::string result;
    int used = 0;
    for (const auto& glyph : Utf8ToGlyphs(std::string(raw))) {
        const int width = string_width(glyph);
        if (used + width > columns) break;
        result += glyph;
        used += width;
    }
    return result;
}

} // namespace

Element RenderRuntimeAgentRow(const Instance& instance, int columns) {
    const auto& palette = *design::theme::current_theme().palette;
    const auto& figures = design::figures::figures();
    const auto foreground = instance.status == Status::Failed ? palette.danger
        : instance.status == Status::Running || instance.status == Status::Completed ? palette.success
        : instance.status == Status::Cancelled ? palette.warning : palette.muted;
    const auto glyph = instance.status == Status::Completed ? figures.tick
        : instance.status == Status::Failed ? figures.cross : figures.circle;
    const int width = std::max(0, columns);
    auto prefix = clipped_line(glyph, std::min(width, 2));
    if (width > string_width(prefix)) prefix += " ";
    const int remaining = std::max(0, width - string_width(prefix));
    const auto state = clipped_line(status_name(instance.status), remaining);
    const int label_columns = std::max(0, remaining - string_width(state) - 1);
    const auto label = clipped_line(instance.name.empty()
        ? (instance.description.empty() ? instance.agent_id : instance.description) : instance.name, label_columns);
    Elements parts{text(prefix) | color(foreground)};
    if (!label.empty()) {
        parts.push_back(text(label) | color(palette.text));
        parts.push_back(text(" "));
    }
    parts.push_back(text(state) | color(foreground));
    return hbox(std::move(parts)) | size(WIDTH, EQUAL, width);
}

Element RenderRuntimeAgentActivity(
    const std::vector<Instance>& instances,
    const std::vector<todos::TodoDisplayItem>& todo_items,
    const std::vector<teams::live::LiveTeammate>& teammates,
    int columns, int budget, bool todos_collapsed, std::string_view response_prefix, ActivityHitTargets* hit_targets) {
    if (hit_targets) { hit_targets->subagents_valid = false; hit_targets->teams_valid = false; }
    if (budget <= 0) return vbox({});
    columns = std::max(0, columns);
    const std::array<bool, 3> present{!todo_items.empty(), !instances.empty(), !teammates.empty()};
    const bool task_only = instances.empty() && teammates.empty();
    const bool completed_tasks = !todo_items.empty() && std::ranges::all_of(todo_items, [](const auto& item) {
        return item.status == todos::TodoDisplayItem::Status::Completed;
    });
    // Priorities are running, unknown team state, queued, pending, folded history.
    // Each array entry remains an index into its original state-owned group.
    std::array<std::array<std::vector<std::size_t>, 3>, 5> candidates;
    std::array<int, 3> priority{4, 4, 4};
    for (std::size_t i = 0; i < todo_items.size(); ++i) {
        const auto status = todo_items[i].status;
        if (status == todos::TodoDisplayItem::Status::InProgress) {
            candidates[0][0].push_back(i);
            priority[0] = 0;
        } else if (completed_tasks) {
            if (!todos_collapsed) candidates[4][0].push_back(i);
        } else if (status == todos::TodoDisplayItem::Status::Pending || task_only) {
            candidates[3][0].push_back(i);
            if (status == todos::TodoDisplayItem::Status::Pending) priority[0] = std::min(priority[0], 3);
        }
    }
    for (std::size_t i = 0; i < instances.size(); ++i) {
        const auto status = instances[i].status;
        const int rank = status == Status::Running ? 0 : status == Status::Queued ? 2 : 4;
        priority[1] = std::min(priority[1], rank);
        if (rank < 4) candidates[static_cast<std::size_t>(rank)][1].push_back(i);
    }
    for (std::size_t i = 0; i < teammates.size(); ++i) {
        const int rank = teammates[i].status == "running" ? 0 : teammates[i].status == "idle" ? 4 : 1;
        priority[2] = std::min(priority[2], rank);
        if (rank < 4) candidates[static_cast<std::size_t>(rank)][2].push_back(i);
    }
    std::array<bool, 3> selected{};
    int remaining = budget;
    for (int rank = 0; rank < 5; ++rank)
        for (std::size_t group = 0; group < present.size() && remaining > 0; ++group)
            if (present[group] && priority[group] == rank) {
                selected[group] = true;
                --remaining;
            }
    std::array<std::vector<std::size_t>, 3> details;
    for (const auto& level : candidates) {
        for (std::size_t round = 0; remaining > 0; ++round) {
            bool found = false;
            for (std::size_t group = 0; group < selected.size() && remaining > 0; ++group)
                if (selected[group] && round < level[group].size()) {
                    details[group].push_back(level[group][round]);
                    --remaining;
                    found = true;
                }
            if (!found) break;
        }
    }

    const int envelope = string_width(std::string(response_prefix));
    const int heading_columns = std::max(0, columns - envelope);
    const int child_columns = std::max(0, heading_columns - 2);
    Elements rows;
    for (std::size_t group = 0; group < selected.size(); ++group) {
        if (!selected[group]) continue;
        const auto heading_prefix = rows.empty() ? std::string(response_prefix) : std::string(envelope, ' ');
        const auto label = group == 0 ? task_summary(todo_items, todos_collapsed)
            : group == 1 ? subagent_summary(instances, heading_columns) : team_summary(teammates, heading_columns);
        auto heading = hbox({text(fitting_prefix(heading_prefix, columns)) | color(design::theme::current_theme().palette->muted),
            summary_row(label, heading_columns)}) | size(WIDTH, EQUAL, columns);
        if (hit_targets && columns > 0 && group == 1) {
            heading = heading | reflect(hit_targets->subagents);
            hit_targets->subagents_valid = true;
        } else if (hit_targets && columns > 0 && group == 2) {
            heading = heading | reflect(hit_targets->teams);
            hit_targets->teams_valid = true;
        }
        rows.push_back(std::move(heading));
        for (const auto index : details[group]) {
            Element item;
            if (group == 0) {
                auto task = todo_items[index];
                task.content = clipped_line(task.content, child_columns);
                item = todos::RenderTodoRow(task, child_columns);
            } else if (group == 1) item = RenderRuntimeAgentRow(instances[index], child_columns);
            else {
                auto teammate = teammates[index];
                teammate.name = clipped_line(teammate.name, child_columns);
                teammate.status = clipped_line(teammate.status, child_columns);
                teammate.last_output_tail = clipped_line(teammate.last_output_tail, child_columns);
                item = teams::live::RenderLiveTeammateRow(teammate, child_columns);
            }
            rows.push_back(hbox({text(fitting_prefix(std::string(envelope + 2, ' '), columns)), std::move(item)})
                | size(WIDTH, EQUAL, columns));
        }
    }
    return vbox(std::move(rows));
}

} // namespace loom::ui::agents::runtime
