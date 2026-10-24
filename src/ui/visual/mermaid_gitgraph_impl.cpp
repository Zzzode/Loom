module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;
import loom.serdes.json;

namespace loom::ui::mermaid::detail {
namespace {

constexpr std::size_t kGitCommitLimit = 64u;
constexpr std::size_t kGitBranchLimit = 32u;
constexpr std::size_t kGitTagLimit = 8u;
constexpr std::size_t kGitParentLimit = 2u;

// RFC 0031 E5: the single parse entry fee dominates the former per-command
// charges (commit/branch allocations, bounded id/reset scans, at most one
// init directive and the per-line numbering sum) for every grammar-feasible
// source under the 64-commit/32-branch caps.
constexpr std::size_t kGitParsePerByte = 12u;
constexpr std::size_t kGitParseFixed = 65536u;

std::string_view trim_git(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) value.remove_suffix(1);
    return value;
}

bool word_boundary(std::string_view value, std::size_t count) {
    return value.size() == count || value[count] == ' ' || value[count] == '\t';
}

std::optional<std::string> git_label(std::string_view raw) {
    const auto text = validated_plain_label(raw);
    if (!text) return {};
    const auto glyphs = ftxui::Utf8ToGlyphs(*text);
    if (glyphs.empty() || glyphs.size() != static_cast<std::size_t>(ftxui::string_width(*text))) return {};
    return text;
}

struct Cursor {
    std::string_view rest;

    void space() { rest = trim_git(rest); }

    bool take(std::string_view token) {
        space();
        if (!rest.starts_with(token)) return false;
        rest.remove_prefix(token.size());
        return true;
    }

    bool word(std::string_view token) {
        space();
        if (!rest.starts_with(token) || !word_boundary(rest, token.size())) return false;
        rest.remove_prefix(token.size());
        return true;
    }

    std::optional<std::string> quoted() {
        space();
        if (rest.size() < 2u || rest.front() != '"') return {};
        const auto end = rest.find('"', 1u);
        if (end == std::string_view::npos || rest.substr(1u, end - 1u).find('\\') != std::string_view::npos) return {};
        auto value = git_label(rest.substr(1u, end - 1u));
        if (!value) return {};
        rest.remove_prefix(end + 1u);
        return value;
    }

    std::optional<std::string> reference() {
        space();
        if (rest.starts_with('"')) return quoted();
        std::size_t count = 0u;
        while (count < rest.size() && rest[count] != ' ' && rest[count] != '\t') {
            const auto c = static_cast<unsigned char>(rest[count]);
            // `:` terminates a direction token.  Stop before it so
            // `gitGraph TB:` leaves the suffix available for the header
            // parser.  Other structural bytes make the whole reference
            // invalid rather than being silently skipped.
            if (c == ':') break;
            if (c < 0x21u || std::string_view{"[]{}()<>`\\\""}.find(static_cast<char>(c)) != std::string_view::npos) return {};
            ++count;
        }
        if (count == 0u) return {};
        auto value = git_label(rest.substr(0u, count));
        if (!value) return {};
        rest.remove_prefix(count);
        return value;
    }

    std::optional<std::string> decimal() {
        space();
        std::size_t count = 0u;
        while (count < rest.size() && rest[count] >= '0' && rest[count] <= '9') ++count;
        if (count == 0u || (count < rest.size() && rest[count] != ' ' && rest[count] != '\t')) return {};
        std::string value(rest.substr(0u, count));
        rest.remove_prefix(count);
        return value;
    }

    bool done() {
        space();
        return rest.empty();
    }
};

std::optional<std::size_t> branch_index(const GitGraph& graph, std::string_view name) {
    for (std::size_t index = 0u; index < graph.branches.size(); ++index)
        if (graph.branches[index].name == name) return index;
    return {};
}

std::optional<std::size_t> commit_index(const GitGraph& graph, std::string_view id) {
    for (std::size_t index = 0u; index < graph.commits.size(); ++index)
        if (graph.commits[index].id == id) return index;
    return {};
}

std::optional<std::string> next_automatic_id(GitGraph& graph) {
    // RFC 0031 E5: bounded by kGitCommitLimit attempts; the parse entry fee
    // dominates the former per-attempt charge.
    for (std::size_t attempts = 0u; attempts < kGitCommitLimit; ++attempts) {
        const auto candidate = std::to_string(graph.automatic_id++);
        if (!commit_index(graph, candidate)) return candidate;
    }
    return {};
}

bool add_commit(GitGraph& graph, std::string id, GitType type, std::vector<std::string> tags,
                std::vector<std::string> parents, std::string_view operation) {
    if (graph.commits.size() >= kGitCommitLimit || parents.size() > kGitParentLimit ||
        tags.size() > kGitTagLimit) return false;
    const auto current = branch_index(graph, graph.current_branch);
    if (!current || id.empty() || commit_index(graph, id)) return false;
    for (const auto& parent : parents) if (!commit_index(graph, parent)) return false;
    GitCommit commit;
    commit.id = std::move(id);
    commit.branch = graph.current_branch;
    commit.operation = std::string(operation);
    commit.type = type;
    commit.tags = std::move(tags);
    commit.parents = std::move(parents);
    commit.sequence = graph.commits.size();
    graph.branches[*current].head = commit.id;
    graph.commits.push_back(std::move(commit));
    graph.commit_after_reset = false;
    return true;
}

std::optional<GitType> parse_type(Cursor& cursor) {
    const auto raw = cursor.reference();
    if (!raw) return {};
    if (*raw == "NORMAL") return GitType::Normal;
    if (*raw == "REVERSE") return GitType::Reverse;
    if (*raw == "HIGHLIGHT") return GitType::Highlight;
    return {};
}

bool parse_commit(GitGraph& graph, std::string_view line) {
    Cursor cursor{line};
    if (!cursor.word("commit")) return false;
    std::optional<std::string> id;
    std::optional<GitType> type;
    std::vector<std::string> tags;
    while (!cursor.done()) {
        if (cursor.take("id:")) {
            if (id) return false;
            id = cursor.quoted();
            if (!id) return false;
        } else if (cursor.take("type:")) {
            if (type) return false;
            type = parse_type(cursor);
            if (!type) return false;
        } else if (cursor.take("tag:")) {
            auto tag = cursor.quoted();
            if (!tag || tags.size() == kGitTagLimit) return false;
            tags.push_back(std::move(*tag));
        } else {
            // The complete selected corpus has no message field. Keep an
            // unsupported message source intact rather than omit its text.
            return false;
        }
    }
    if (!id) id = next_automatic_id(graph);
    if (!id) return false;
    std::vector<std::string> parents;
    const auto current = branch_index(graph, graph.current_branch);
    if (!current) return false;
    if (graph.branches[*current].head != "∅") parents.push_back(graph.branches[*current].head);
    return add_commit(graph, std::move(*id), type.value_or(GitType::Normal), std::move(tags), std::move(parents),
                      graph.commit_after_reset ? "commit_after_reset" : "commit");
}

bool parse_branch(GitGraph& graph, std::string_view line) {
    Cursor cursor{line};
    if (!cursor.word("branch")) return false;
    auto name = cursor.reference();
    if (!name || branch_index(graph, *name) || graph.branches.size() >= kGitBranchLimit) return false;
    int order = -1;
    if (!cursor.done()) {
        if (!cursor.take("order:")) return false;
        const auto value = cursor.decimal();
        if (!value || !cursor.done()) return false;
        const auto parsed = std::from_chars(value->data(), value->data() + value->size(), order);
        if (parsed.ec != std::errc{} || parsed.ptr != value->data() + value->size() || order < 0) return false;
    }
    const auto current = branch_index(graph, graph.current_branch);
    if (!current) return false;
    // A branch starts at, and initially points to, the current branch head.
    // Retaining both values closes the first child commit's parent relation.
    const auto inherited_head = graph.branches[*current].head;
    graph.branches.push_back({std::move(*name), inherited_head, inherited_head, graph.branches.size(), order});
    graph.current_branch = graph.branches.back().name;
    return true;
}

bool parse_checkout(GitGraph& graph, std::string_view line) {
    Cursor cursor{line};
    if (!(cursor.word("checkout") || cursor.word("switch"))) return false;
    auto name = cursor.reference();
    if (!name || !cursor.done() || !branch_index(graph, *name)) return false;
    graph.current_branch = std::move(*name);
    return true;
}

bool parse_merge(GitGraph& graph, std::string_view line) {
    Cursor cursor{line};
    if (!cursor.word("merge")) return false;
    auto source = cursor.reference();
    if (!source) return false;
    std::optional<std::string> id;
    std::optional<GitType> type;
    std::vector<std::string> tags;
    while (!cursor.done()) {
        if (cursor.take("id:")) {
            if (id) return false;
            id = cursor.quoted();
            if (!id) return false;
        } else if (cursor.take("type:")) {
            if (type) return false;
            type = parse_type(cursor);
            if (!type) return false;
        } else if (cursor.take("tag:")) {
            auto tag = cursor.quoted();
            if (!tag || tags.size() == kGitTagLimit) return false;
            tags.push_back(std::move(*tag));
        } else return false;
    }
    const auto current = branch_index(graph, graph.current_branch);
    const auto merged = branch_index(graph, *source);
    if (!current || !merged || *current == *merged || graph.branches[*current].head == "∅" || graph.branches[*merged].head == "∅") return false;
    if (!id) id = next_automatic_id(graph);
    if (!id) return false;
    return add_commit(graph, std::move(*id), type.value_or(GitType::Normal), std::move(tags),
                      {graph.branches[*current].head, graph.branches[*merged].head}, "merge");
}

bool parse_cherry_pick(GitGraph& graph, std::string_view line) {
    Cursor cursor{line};
    if (!cursor.word("cherry-pick")) return false;
    std::optional<std::string> source_id, parent;
    std::vector<std::string> tags;
    while (!cursor.done()) {
        if (cursor.take("id:")) {
            if (source_id) return false;
            source_id = cursor.quoted();
            if (!source_id) return false;
        } else if (cursor.take("parent:")) {
            if (parent) return false;
            parent = cursor.quoted();
            if (!parent) return false;
        } else if (cursor.take("tag:")) {
            auto tag = cursor.quoted();
            if (!tag || tags.size() == kGitTagLimit) return false;
            tags.push_back(std::move(*tag));
        } else return false;
    }
    if (!source_id) return false;
    const auto source = commit_index(graph, *source_id);
    const auto current = branch_index(graph, graph.current_branch);
    if (!source || !current || graph.branches[*current].head == "∅" || graph.commits[*source].parents.empty()) return false;
    if (parent && std::ranges::find(graph.commits[*source].parents, *parent) == graph.commits[*source].parents.end()) return false;
    auto id = graph.commits[*source].id + "-cherry";
    if (commit_index(graph, id)) return false;
    return add_commit(graph, std::move(id), GitType::Normal, std::move(tags),
                      {graph.branches[*current].head, graph.commits[*source].id}, "cherry_pick");
}

bool parse_reset(GitGraph& graph, std::string_view line) {
    Cursor cursor{line};
    if (!cursor.word("reset")) return false;
    auto target = cursor.reference();
    if (!target || !cursor.done()) return false;
    std::size_t depth = 0u;
    const auto separator = target->find('~');
    std::string branch = separator == std::string::npos ? *target : target->substr(0u, separator);
    if (separator != std::string::npos) {
        const auto suffix = std::string_view(*target).substr(separator + 1u);
        if (suffix.empty() || !std::ranges::all_of(suffix, [](unsigned char ch) { return ch >= '0' && ch <= '9'; })) return false;
        const auto result = std::from_chars(suffix.data(), suffix.data() + suffix.size(), depth);
        if (result.ec != std::errc{} || result.ptr != suffix.data() + suffix.size() || depth == 0u) return false;
    }
    const auto current = branch_index(graph, graph.current_branch);
    const auto source = branch_index(graph, branch);
    if (!current || !source || graph.branches[*source].head == "∅") return false;
    std::string head = graph.branches[*source].head;
    while (depth-- > 0u) {
        // Bounded by the existing commit chain (<= kGitCommitLimit links);
        // a missing ancestor rejects. RFC 0031 E5 folds the per-step charge
        // into the parse entry fee.
        const auto index = commit_index(graph, head);
        if (!index || graph.commits[*index].parents.empty()) return false;
        head = graph.commits[*index].parents.front();
    }
    graph.branches[*current].head = std::move(head);
    graph.commit_after_reset = true;
    return true;
}

bool parse_init(GitGraph& graph, std::string_view line) {
    line = trim_git(line);
    constexpr std::string_view prefix = "%%{init:";
    if (!line.starts_with(prefix) || !line.ends_with("}%%")) return false;
    const auto document = loom::utils::json::parse(std::string(line.substr(prefix.size(), line.size() - prefix.size() - 3u)));
    if (!document || !document->root().is_obj()) return false;
    bool valid = true, seen_name = false, seen_commit_label = false;
    std::size_t root_fields = 0u, graph_fields = 0u;
    document->root().iter_obj([&](loom::utils::json::JsonVal key, loom::utils::json::JsonVal value) {
        ++root_fields;
        if (key.as_str() != "gitGraph" || !value.is_obj()) { valid = false; return; }
        value.iter_obj([&](loom::utils::json::JsonVal child_key, loom::utils::json::JsonVal child_value) {
            ++graph_fields;
            if (child_key.as_str() == "mainBranchName") {
                if (seen_name || !child_value.is_str()) { valid = false; return; }
                seen_name = true;
                const auto name = git_label(child_value.as_str());
                if (!name || *name == "∅") { valid = false; return; }
                graph.main_branch = *name;
                graph.current_branch = *name;
            } else if (child_key.as_str() == "showCommitLabel") {
                // The explicit true setting has the same visible semantics as
                // this ID-caption profile. False needs a separate hidden-ID profile.
                if (seen_commit_label || !child_value.is_bool() || !child_value.as_bool()) { valid = false; return; }
                seen_commit_label = true;
            } else valid = false;
        });
    });
    return valid && root_fields == 1u && graph_fields >= 1u && graph_fields <= 2u;
}

} // namespace

std::optional<GitGraph> parse_gitgraph(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    if (source.size() > (std::numeric_limits<std::size_t>::max() - kGitParseFixed) / kGitParsePerByte ||
        !work.spend(source.size() * kGitParsePerByte + kGitParseFixed)) return {};
    GitGraph graph;
    if (!work.spend(kGitCommitLimit * (sizeof(GitCommit) + 192u) + kGitBranchLimit * (sizeof(GitBranch) + 128u))) return {};
    graph.commits.reserve(kGitCommitLimit);
    graph.branches.reserve(kGitBranchLimit);

    bool seen_header = false, seen_init = false;
    while (!source.empty()) {
        const auto end = source.find('\n');
        auto line = source.substr(0u, end);
        source = end == std::string_view::npos ? std::string_view{} : source.substr(end + 1u);
        line = trim_git(line);
        if (line.empty()) continue;
        // A directive changes graph semantics.  It is accepted only in the
        // pre-header position and only for the one fully represented init
        // shape below; a generic comment remains non-semantic.
        if (line.starts_with("%%{")) {
            if (seen_header || seen_init || !line.starts_with("%%{init:") || !parse_init(graph, line)) return {};
            seen_init = true;
            continue;
        }
        if (line.starts_with("%%")) continue;
        if (!seen_header) {
            Cursor header{line};
            header.space();
            constexpr std::string_view marker = "gitGraph";
            if (!header.rest.starts_with(marker)) return {};
            header.rest.remove_prefix(marker.size());
            if (!header.rest.empty() && header.rest.front() != ':' && header.rest.front() != ' ' && header.rest.front() != '\t') return {};
            header.space();
            if (header.take(":")) {
                if (!header.done()) return {};
            } else if (!header.done()) {
                const auto direction = header.reference();
                if (!direction || !header.take(":") || !header.done()) return {};
                if (*direction == "LR") graph.direction = GitDirection::LR;
                else if (*direction == "TB") graph.direction = GitDirection::TB;
                else if (*direction == "BT") graph.direction = GitDirection::BT;
                else return {};
            }
            graph.branches.push_back({graph.main_branch, "∅", "∅", 0u});
            seen_header = true;
            continue;
        }
        bool parsed = parse_commit(graph, line) || parse_branch(graph, line) ||
                      parse_checkout(graph, line) || parse_merge(graph, line) ||
                      parse_cherry_pick(graph, line) || parse_reset(graph, line);
        if (!parsed) return {};
    }
    if (!seen_header || graph.commit_after_reset || graph.branches.empty()) return {};
    return graph;
}

namespace {

std::string marker(GitType type, bool use_ascii) {
    if (use_ascii) return type == GitType::Normal ? "o" : type == GitType::Reverse ? "X" : "#";
    if (type == GitType::Normal) return "●";
    if (type == GitType::Reverse) return "✖";
    return "■";
}

std::string commit_caption(const GitCommit& commit) {
    return commit.id;
}

std::string branch_caption(const GitBranch& branch, const GitGraph& graph, bool use_ascii) {
    return std::string(branch.name == graph.current_branch ? "*" : "") +
           (branch.name == graph.main_branch ? "@" : "") + branch.name + (use_ascii ? " > " : " → ") +
           (use_ascii && branch.head == "∅" ? "-" : branch.head);
}

int cells(std::string_view text) { return ftxui::string_width(std::string(text)); }

bool checked_dimension(std::size_t width, std::size_t height, int& out_width, int& out_height, Work& work) {
    if (width == 0u || height == 0u || width > static_cast<std::size_t>(kWidthLimit) || height > static_cast<std::size_t>(kHeightLimit)) return false;
    if (width > std::numeric_limits<std::size_t>::max() / height) return false;
    const auto count = width * height;
    if (count > std::numeric_limits<std::size_t>::max() / (sizeof(Cell) + 32u) ||
        !work.spend(count * (sizeof(Cell) + 32u) + height * (sizeof(std::string) + width + 32u) + 4096u)) return false;
    out_width = static_cast<int>(width);
    out_height = static_cast<int>(height);
    return true;
}

struct ParentRoutePlan { std::vector<std::vector<Point>> paths; };

bool reserve_parent_span(std::vector<unsigned char>& occupied, int width, int height, Point start, int count) {
    if (count <= 0 || start.x < 0 || start.y < 0 || start.y >= height || start.x > width - count) return false;
    for (int offset = 0; offset < count; ++offset) {
        const auto index = static_cast<std::size_t>(start.y * width + start.x + offset);
        if (occupied[index] != 0u) return false;
        occupied[index] = 1u;
    }
    return true;
}

std::optional<ParentRoutePlan> plan_parent_routes(
    const GitGraph& graph, const std::vector<Point>& positions,
    const std::vector<std::pair<std::size_t, std::size_t>>& parents,
    const std::vector<std::string>& captions, const std::vector<Point>& branch_guides,
    int width, int height, Work& work, bool use_ascii, bool horizontal_flow) {
    if (width <= 0 || height <= 0 || positions.size() != graph.commits.size() ||
        captions.size() != graph.commits.size() || branch_guides.size() != graph.branches.size()) return {};
    const auto grid_cells = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    constexpr std::size_t kWorkspaceUnit = 2u * sizeof(unsigned char) + 2u * sizeof(int) + 16u;
    if (grid_cells > std::numeric_limits<std::size_t>::max() / kWorkspaceUnit ||
        parents.size() > std::numeric_limits<std::size_t>::max() / grid_cells) return {};
    const auto searches = grid_cells * parents.size();
    std::size_t pass_fee = grid_cells * kWorkspaceUnit;
    if (searches > (std::numeric_limits<std::size_t>::max() - pass_fee) / 4u) return {};
    pass_fee += searches * 4u;
    constexpr std::size_t kRouteSlotFee = sizeof(std::vector<Point>) + 64u;
    if (pass_fee > std::numeric_limits<std::size_t>::max() - 4096u ||
        parents.size() > (std::numeric_limits<std::size_t>::max() - pass_fee - 4096u) / kRouteSlotFee) return {};
    pass_fee += parents.size() * kRouteSlotFee + 4096u;

    const auto index_of = [&](Point point) { return static_cast<std::size_t>(point.y * width + point.x); };
    constexpr std::array<Point, 4> kSteps{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};

    // Commit markers (2) are permanent obstacles; reserved caption/tag/branch
    // spans (1) keep connectors out of painted labels. The reservation is
    // identical for both passes, so it is rebuilt from scratch on retry.
    const auto build_workspace = [&]() -> std::optional<std::pair<std::vector<unsigned char>,
                                                                  std::vector<unsigned char>>> {
        std::vector<unsigned char> occupied(grid_cells, 0u), routed(grid_cells, 0u);
        for (const auto marker : positions) {
            if (marker.x < 0 || marker.y < 0 || marker.x >= width || marker.y >= height ||
                occupied[index_of(marker)] != 0u)
                return {};
            occupied[index_of(marker)] = 2u;
        }
        for (std::size_t index = 0u; index < graph.commits.size(); ++index) {
            const auto marker = positions[index];
            if (!reserve_parent_span(occupied, width, height, {marker.x + 3, marker.y}, cells(captions[index]))) return {};
            for (std::size_t tag = 0u; tag < graph.commits[index].tags.size(); ++tag) {
                const auto tag_width = cells(graph.commits[index].tags[tag]) + 2;
                if (!reserve_parent_span(occupied, width, height,
                                         {marker.x + 3, marker.y + 1 + static_cast<int>(tag)}, tag_width)) return {};
            }
        }
        for (std::size_t index = 0u; index < graph.branches.size(); ++index) {
            const auto body = branch_caption(graph.branches[index], graph, use_ascii);
            const auto guide = branch_guides[index];
            const Point caption{guide.x + 3, guide.y};
            // Keep parent paths out of the lane guide, separator and complete ref.
            if (!reserve_parent_span(occupied, width, height, {caption.x - 3, caption.y}, 3) ||
                !reserve_parent_span(occupied, width, height, caption, cells(body))) return {};
        }
        return std::pair{std::move(occupied), std::move(routed)};
    };

    // One whole-plan routing pass. Pass 1 (strict) routes edges in commit
    // sequence order, treats every reserved/laid cell as an obstacle and keeps
    // a one-cell moat around every prior route, so connectors never cross,
    // touch, or share. Pass 2 (relaxed) routes the same-lane commit trunks
    // first and may run adjacent to or through an earlier connector; commit
    // markers and painted-label spans stay sealed in both passes.
    //
    // The BFS scratch arrays are allocated once by the caller: the strict
    // prepayment already covers them, so the retry rebuilds only the two byte
    // workspaces. The relaxed pass additionally meters its BFS expansions as
    // they happen (four units per dequeued cell, at most grid * edges in
    // total) instead of prepaying the full worst-case search a second time.
    std::vector<int> previous(grid_cells, -1);
    std::vector<int> queue;
    queue.reserve(grid_cells);
    const auto run_pass = [&](std::vector<unsigned char>& occupied, std::vector<unsigned char>& routed,
                              bool strict) -> std::optional<ParentRoutePlan> {
        ParentRoutePlan plan;
        plan.paths.reserve(parents.size());
        std::vector<std::size_t> order(parents.size());
        std::iota(order.begin(), order.end(), std::size_t{0u});
        if (!strict) {
            const auto same_lane = [&](std::size_t child, std::size_t parent) {
                return horizontal_flow ? positions[child].y == positions[parent].y
                                       : positions[child].x == positions[parent].x;
            };
            // Stable partition: lane trunks before cross-lane connectors, each
            // group still in commit sequence. Laying the spines first keeps
            // the backward merges and second parents from sealing the trunk.
            std::stable_sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
                return same_lane(parents[left].first, parents[left].second) >
                       same_lane(parents[right].first, parents[right].second);
            });
        }
        const auto touches_prior_route = [&](Point point) {
            for (const auto step : kSteps) {
                const Point neighbor{point.x + step.x, point.y + step.y};
                if (neighbor.x >= 0 && neighbor.y >= 0 && neighbor.x < width && neighbor.y < height &&
                    routed[index_of(neighbor)] != 0u) return true;
            }
            return false;
        };
        for (const std::size_t edge : order) {
            const auto [child, parent] = parents[edge];
            if (child >= positions.size() || parent >= positions.size() || child == parent) return {};
            const int source = static_cast<int>(index_of(positions[child]));
            const int target = static_cast<int>(index_of(positions[parent]));
            std::fill(previous.begin(), previous.end(), -1);
            queue.clear();
            queue.push_back(source);
            previous[static_cast<std::size_t>(source)] = source;
            for (std::size_t read = 0u; read < queue.size(); ++read) {
                const int current = queue[read];
                if (current == target) break;
                if (!strict && !work.spend(4u)) return {};
                const Point here{current % width, current / width};
                for (const auto step : kSteps) {
                    const Point next{here.x + step.x, here.y + step.y};
                    if (next.x < 0 || next.y < 0 || next.x >= width || next.y >= height) continue;
                    const int next_index = static_cast<int>(index_of(next));
                    if (previous[static_cast<std::size_t>(next_index)] >= 0) continue;
                    if (next_index != target) {
                        const auto cell = static_cast<std::size_t>(next_index);
                        if (strict) {
                            if (occupied[cell] != 0u || touches_prior_route(next)) continue;
                        } else {
                            // Markers and painted-label spans stay sealed; an
                            // already-laid connector cell may be reused.
                            if (occupied[cell] == 2u || (occupied[cell] == 1u && routed[cell] == 0u)) continue;
                        }
                    }
                    previous[static_cast<std::size_t>(next_index)] = current;
                    queue.push_back(next_index);
                }
            }
            if (previous[static_cast<std::size_t>(target)] < 0) return {};
            std::size_t length = 1u;
            for (int current = target; current != source; current = previous[static_cast<std::size_t>(current)]) ++length;
            if (length > (std::numeric_limits<std::size_t>::max() - 64u) / (sizeof(Point) + 16u) ||
                !work.spend(length * (sizeof(Point) + 16u) + 64u)) return {};
            std::vector<Point> route;
            route.reserve(length);
            for (int current = target;; current = previous[static_cast<std::size_t>(current)]) {
                route.push_back({current % width, current / width});
                if (current == source) break;
            }
            std::ranges::reverse(route);
            for (const auto point : route) {
                const auto point_index = index_of(point);
                if (point_index == static_cast<std::size_t>(source) || point_index == static_cast<std::size_t>(target)) continue;
                if (strict) {
                    if (occupied[point_index] != 0u) return {};
                } else if (occupied[point_index] == 2u ||
                           (occupied[point_index] == 1u && routed[point_index] == 0u)) {
                    return {};
                }
                occupied[point_index] = 1u;
                routed[point_index] = 1u;
            }
            plan.paths.push_back(std::move(route));
        }
        return plan;
    };

    // Pass 1: the exact strict plan. Every input that routes today must keep
    // this plan byte-for-byte; the relaxed retry exists only for full failure.
    if (!work.spend(pass_fee)) return {};
    auto workspace = build_workspace();
    if (!workspace) return {};
    if (auto plan = run_pass(workspace->first, workspace->second, true)) return plan;

    // Pass 2: one deterministic bounded whole-plan retry that permits
    // route-cell crossing after strict failure. Its output is consumable only
    // because pass 1 failed; pass-1 success never reaches this branch. The
    // retry rebuilds only the byte workspaces (the int BFS scratch is reused)
    // and pays for its actual, grid*edges-bounded expansions per dequeue.
    constexpr std::size_t kRelaxedUnit = 2u * sizeof(unsigned char) + 16u;
    std::size_t relaxed_fee = grid_cells * kRelaxedUnit;
    if (relaxed_fee > std::numeric_limits<std::size_t>::max() - 4096u ||
        parents.size() > (std::numeric_limits<std::size_t>::max() - relaxed_fee - 4096u) / kRouteSlotFee) return {};
    relaxed_fee += parents.size() * kRouteSlotFee + 4096u;
    if (!work.spend(relaxed_fee)) return {};
    workspace = build_workspace();
    if (!workspace) return {};
    return run_pass(workspace->first, workspace->second, false);
}

bool draw_parent_routes(Canvas& canvas, const ParentRoutePlan& plan, bool use_ascii) {
    for (const auto& route : plan.paths) {
        if (route.size() < 2u) return false;
        for (std::size_t point = 1u; point < route.size(); ++point)
            if (!canvas.segment(route[point - 1u], route[point])) return false;
    }
    if (use_ascii) {
        // Only route cells carry bits at this phase. Project geometry before
        // markers and literal labels are painted, preserving Unicode labels.
        if (!canvas.work.spend(canvas.cells.size())) return false;
        for (auto& cell : canvas.cells) if (cell.bits) {
            const bool horizontal = (cell.bits & ~(kLeft | kRight)) == 0u;
            const bool vertical = (cell.bits & ~(kUp | kDown)) == 0u;
            cell.glyph = horizontal ? "-" : vertical ? "|" : "+";
        }
    }
    return true;
}

bool draw_branch_refs(Canvas& canvas, const GitGraph& graph, const std::vector<Point>& branch_guides,
                      Work& work, bool use_ascii) {
    for (std::size_t index = 0u; index < graph.branches.size(); ++index) {
        if (!work.spend(128u)) return false;
        const auto body = branch_caption(graph.branches[index], graph, use_ascii);
        const auto guide = branch_guides[index];
        const Point caption{guide.x + 3, guide.y};
        if (!canvas.put(guide, use_ascii ? "|" : "│") || !canvas.text(caption, body)) return false;
    }
    return true;
}

std::optional<std::vector<std::string>> layout_gitgraph(const GitGraph& graph, Work& work, bool use_ascii) {
    if (graph.commits.empty()) return std::vector<std::string>{std::string{}};
    if (!work.spend(graph.commits.size() * (sizeof(Point) + 256u) + graph.branches.size() * (sizeof(int) + sizeof(Point) + 128u))) return {};
    if (!work.spend(graph.branches.size() * graph.branches.size() *
                    (4u * sizeof(std::size_t) + 4u * sizeof(int) + 16u) + 128u)) return {};
    std::vector<int> lane(graph.branches.size(), 0);
    // The pinned Mermaid AST assigns unspecified branches the decimal 0.i
    // (i is their declaration index), then stably sorts the numeric order.
    // Compare exact rational keys instead of floats; the bounded branch count
    // keeps the denominators and signed 64-bit cross-products within range.
    const auto order_key = [](const GitBranch& branch) {
        if (branch.order >= 0)
            return std::pair<std::int64_t, std::int64_t>{branch.order, 1};
        std::int64_t denominator = 1;
        for (auto ordinal = branch.declaration; ordinal; ordinal /= 10u) denominator *= 10;
        return std::pair<std::int64_t, std::int64_t>{static_cast<std::int64_t>(branch.declaration), denominator};
    };
    const auto precedes = [&](std::size_t left, std::size_t right) {
        const auto& a = graph.branches[left];
        const auto& b = graph.branches[right];
        const auto a_key = order_key(a), b_key = order_key(b);
        const auto a_scaled = a_key.first * b_key.second;
        const auto b_scaled = b_key.first * a_key.second;
        return a_scaled != b_scaled ? a_scaled < b_scaled : a.declaration < b.declaration;
    };
    for (std::size_t index = 0u; index < graph.branches.size(); ++index)
        for (std::size_t other = 0u; other < graph.branches.size(); ++other)
            if (precedes(other, index)) ++lane[index];

    std::vector<std::string> captions;
    captions.reserve(graph.commits.size());
    std::vector<int> caption_width;
    caption_width.reserve(graph.commits.size());
    int max_tags = 0, max_branch_width = 0;
    for (const auto& branch : graph.branches) max_branch_width = std::max(max_branch_width, cells(branch_caption(branch, graph, use_ascii)));
    for (const auto& commit : graph.commits) {
        const auto caption = commit_caption(commit);
        if (!git_label(caption) || cells(caption) > kLabelLimit) return {};
        captions.push_back(caption);
        int extent = cells(caption);
        for (const auto& tag : commit.tags) extent = std::max(extent, cells(tag) + 2);
        caption_width.push_back(extent);
        max_tags = std::max(max_tags, static_cast<int>(commit.tags.size()));
        for (const auto& tag : commit.tags) if (!git_label(tag) || cells("[" + tag + "]") > kLabelLimit) return {};
    }
    const int lane_step = max_tags + 3;
    // Keep successive vertical captions outside the locked three-cell marker radius.
    const int sequence_step = std::max(max_tags + 3, 4);
    std::vector<std::pair<std::size_t, std::size_t>> parents;
    if (!work.spend(graph.commits.size() * kGitParentLimit * (2u * sizeof(std::size_t) + 32u) + 4096u)) return {};
    parents.reserve(graph.commits.size() * kGitParentLimit);
    for (std::size_t child = 0u; child < graph.commits.size(); ++child) for (const auto& parent_id : graph.commits[child].parents) {
        const auto parent = commit_index(graph, parent_id);
        if (!parent || parents.size() == graph.commits.size() * kGitParentLimit) return {};
        parents.emplace_back(child, *parent);
    }

    std::vector<Point> positions(graph.commits.size());
    std::vector<Point> branch_guides;
    branch_guides.reserve(graph.branches.size());
    int width = 0, height = 0;
    if (graph.direction == GitDirection::LR) {
        int cursor = max_branch_width + 8;
        const int route_rows = static_cast<int>(parents.size()) + 2;
        for (const auto branch_lane : lane) branch_guides.push_back({0, route_rows + branch_lane * lane_step});
        for (std::size_t index = 0u; index < graph.commits.size(); ++index) {
            const auto branch = branch_index(graph, graph.commits[index].branch);
            if (!branch) return {};
            positions[index] = {cursor, route_rows + lane[*branch] * lane_step};
            cursor += caption_width[index] + 5;
            width = std::max(width, cursor);
            for (const auto& tag : graph.commits[index].tags) width = std::max(width, positions[index].x + 3 + cells("[" + tag + "]"));
        }
        width = std::max(width, 3 + max_branch_width);
        height = route_rows + static_cast<int>(graph.branches.size()) * lane_step + 2 + static_cast<int>(graph.branches.size());
        int planned_width = 0, planned_height = 0;
        if (!checked_dimension(static_cast<std::size_t>(width), static_cast<std::size_t>(height), planned_width, planned_height, work)) return {};
        const auto route_plan = plan_parent_routes(graph, positions, parents, captions, branch_guides,
                                                   planned_width, planned_height, work, use_ascii, true);
        if (!route_plan) return {};
        Canvas canvas(planned_width, planned_height, work);
        if (!draw_parent_routes(canvas, *route_plan, use_ascii)) return {};
        for (std::size_t index = 0u; index < graph.commits.size(); ++index) {
            if (!canvas.put(positions[index], marker(graph.commits[index].type, use_ascii)) || !canvas.text({positions[index].x + 3, positions[index].y}, captions[index])) return {};
            for (std::size_t tag = 0u; tag < graph.commits[index].tags.size(); ++tag)
                if (!canvas.text({positions[index].x + 3, positions[index].y + 1 + static_cast<int>(tag)}, "[" + graph.commits[index].tags[tag] + "]")) return {};
        }
        if (!draw_branch_refs(canvas, graph, branch_guides, work, use_ascii)) return {};
        return canvas.lines();
    }

    const int route_columns = static_cast<int>(parents.size()) + 2;
    const int lane_width = std::max(6, max_branch_width + 4);
    const int sequence_count = static_cast<int>(graph.commits.size());
    for (std::size_t index = 0u; index < graph.commits.size(); ++index) {
        const auto branch = branch_index(graph, graph.commits[index].branch);
        if (!branch) return {};
        const int y = graph.direction == GitDirection::TB ?
                      static_cast<int>(graph.branches.size()) + 2 + static_cast<int>(index) * sequence_step :
                      (sequence_count - 1 - static_cast<int>(index)) * sequence_step;
        positions[index] = {route_columns + lane[*branch] * lane_width, y};
        width = std::max(width, positions[index].x + 3 + caption_width[index]);
        for (const auto& tag : graph.commits[index].tags) width = std::max(width, positions[index].x + 3 + cells("[" + tag + "]"));
        height = std::max(height, positions[index].y + max_tags + 2);
    }
    // Top-to-bottom labels precede commits; bottom-to-top labels follow them.
    const int state_y = graph.direction == GitDirection::TB ? 0 : sequence_count * sequence_step + 1;
    for (std::size_t index = 0u; index < graph.branches.size(); ++index) {
        branch_guides.push_back({route_columns + lane[index] * lane_width, state_y + static_cast<int>(index)});
        width = std::max(width, branch_guides.back().x + 3 + cells(branch_caption(graph.branches[index], graph, use_ascii)));
    }
    height = std::max(height, state_y + static_cast<int>(graph.branches.size()));
    int planned_width = 0, planned_height = 0;
    if (!checked_dimension(static_cast<std::size_t>(width), static_cast<std::size_t>(height), planned_width, planned_height, work)) return {};
    const auto route_plan = plan_parent_routes(graph, positions, parents, captions, branch_guides,
                                               planned_width, planned_height, work, use_ascii, false);
    if (!route_plan) return {};
    Canvas canvas(planned_width, planned_height, work);
    if (!draw_parent_routes(canvas, *route_plan, use_ascii)) return {};
    for (std::size_t index = 0u; index < graph.commits.size(); ++index) {
        if (!canvas.put(positions[index], marker(graph.commits[index].type, use_ascii)) || !canvas.text({positions[index].x + 3, positions[index].y}, captions[index])) return {};
        for (std::size_t tag = 0u; tag < graph.commits[index].tags.size(); ++tag)
            if (!canvas.text({positions[index].x + 3, positions[index].y + 1 + static_cast<int>(tag)}, "[" + graph.commits[index].tags[tag] + "]")) return {};
    }
    if (!draw_branch_refs(canvas, graph, branch_guides, work, use_ascii)) return {};
    return canvas.lines();
}

} // namespace

// RFC 0031 E2a: gitGraph admits a description-only frontmatter envelope.
// The description is documentation-only: validated and discarded, never
// rendered. A top-level config key, any body init directive, or any title
// (gitGraph has no title projection) keeps the whole source.
std::optional<std::string_view> git_frontmatter_body(std::string_view source, Work& work) {
    const auto envelope = detail::parse_envelope(source, work);
    if (!envelope || !envelope->has_frontmatter || !envelope->init.empty()) return {};
    bool description_seen = false;
    for (const auto& event : envelope->events) {
        if (event.blank || event.comment) continue;
        if (!event.top_level) return {};
        if (event.path == "config") return {};
        if (event.path == "title") return {};
        if (event.path != "description") return {};
        description_seen = true;
    }
    if (!description_seen || !envelope->description) return {};
    const auto& description = *envelope->description;
    if (description.form != EnvelopeScalarForm::Folded || description.header != ">-") return {};
    bool payload = false;
    for (std::size_t at = 0; at < description.text.size();) {
        const auto newline = description.text.find('\n', at);
        const auto line = description.text.substr(at, newline == std::string_view::npos ?
            std::string_view::npos : newline - at);
        at = newline == std::string_view::npos ? description.text.size() : newline + 1u;
        if (line.empty()) continue;
        if (line.size() < 3u || !line.starts_with("  ") || line.starts_with("   ") ||
            line.find('\t') != std::string_view::npos || !unicode_text(line)) return {};
        payload = true;
    }
    if (!payload) return {};
    return envelope->raw_body;
}

std::optional<std::vector<std::string>> gitgraph_layout(std::string_view source, Work& work, bool use_ascii) {
    std::string_view body = source;
    if (!use_ascii && source.starts_with("---")) {
        const auto admitted = git_frontmatter_body(source, work);
        if (!admitted) return {};
        body = *admitted;
    }
    const auto graph = detail::parse_gitgraph(body, work);
    if (!graph) return {};
    return layout_gitgraph(*graph, work, use_ascii);
}

} // namespace loom::ui::mermaid::detail
