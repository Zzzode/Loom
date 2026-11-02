// Separate test-private restricted physical strand domain. No producer geometry.
#pragma once
#include "mermaid_flow_p0_authority.hpp"

namespace loom::test::mermaid_flow::strands {
enum class Outcome { Match, Mismatch, Inconclusive };
enum class Reason {
    None,
    Domain,
    Identity,
    Ownership,
    Direction,
    Marker,
    Port,
    Ink,
    Route,
    Multiplicity,
    Conservation,
    Limit,
    Work,
    Footer,
    Internal
};

struct Result {
    Outcome outcome = Outcome::Inconclusive;
    Reason reason = Reason::Internal;
    bool resource = false;
    std::size_t physical_edges = 0u;
};

struct Limits {
    std::size_t path_cells = 2048u, strands = 14u;
};

inline Result mismatch(Reason reason) {
    return {Outcome::Mismatch, reason, false, 0u};
}

inline Result inconclusive(Reason reason, bool resource = false) {
    return {Outcome::Inconclusive, reason, resource, 0u};
}

inline bool opposite_pair(int bits) {
    return bits == (L | R) || bits == (U | D);
}

inline int other_end(int bits, int incoming) {
    return bits & ~incoming;
}

inline bool one_bit(int bits) {
    return bits > 0 && (bits & (bits - 1)) == 0;
}

inline Point step(Point p, int bit) {
    if (bit == U)
        --p.y;
    else if (bit == D)
        ++p.y;
    else if (bit == L)
        --p.x;
    else if (bit == R)
        ++p.x;
    return p;
}

inline int reverse(int bit) {
    return bit == U ? D : bit == D ? U : bit == L ? R : bit == R ? L : 0;
}

struct Port {
    int node = -1;
    Point border{}, terminal{};
    int outward = 0;
    bool head = false;
};

struct EdgeKey {
    int from = -1, to = -1;
    bool head = false;
    bool operator==(const EdgeKey&) const = default;
};

inline EdgeKey key(int from, int to, bool head) {
    if (!head && to < from) std::swap(from, to);
    return {from, to, head};
}

// Common-owner projection uses authored orientation even for headless edges.
// DFS back edges are suppressed in source order, matching the local rank rule.
inline bool directions(const Model& model, const std::vector<ShapeFrame>& entities,
                       const std::vector<int>& owners, OracleWork& work) {
    struct Projection {
        int from, to, owner;
    };

    work.spend(16384u);
    std::array<Projection, 14> projections{};
    std::array<bool, 14> skip{};
    const auto nodes = static_cast<int>(model.nodes.size()),
               groups = static_cast<int>(model.groups.size());
    const auto parent = [&](int group) {
        return model.groups[static_cast<std::size_t>(group)].parent;
    };
    for (std::size_t i = 0u; i < model.edges.size(); ++i) {
        work.spend(256u);
        const auto& edge = model.edges[i];
        int common = -1;
        for (int a = owners[static_cast<std::size_t>(edge.from)]; a >= 0 && common < 0;
             a = parent(a))
            for (int b = owners[static_cast<std::size_t>(edge.to)]; b >= 0; b = parent(b))
                if (a == b) {
                    common = a;
                    break;
                }
        const auto frontier = [&](int node) {
            int group = owners[static_cast<std::size_t>(node)];
            if (group == common) return node;
            while (group >= 0 && parent(group) != common)
                group = parent(group);
            return group < 0 ? node : nodes + group;
        };
        projections[i] = {frontier(edge.from), frontier(edge.to), common};
    }
    for (int owner = -1; owner < groups; ++owner) {
        std::array<unsigned char, 13> colors{};
        const auto visit = [&](auto&& self, int at) -> void {
            work.spend(256u);
            colors[static_cast<std::size_t>(at)] = 1u;
            for (std::size_t i = 0u; i < model.edges.size(); ++i) {
                const auto p = projections[i];
                if (p.owner != owner || p.from != at || p.from == p.to) continue;
                if (colors[static_cast<std::size_t>(p.to)] == 1u)
                    skip[i] = true;
                else if (colors[static_cast<std::size_t>(p.to)] == 0u)
                    self(self, p.to);
            }
            colors[static_cast<std::size_t>(at)] = 2u;
        };
        for (int at = 0; at < nodes + groups; ++at)
            if (colors[static_cast<std::size_t>(at)] == 0u) visit(visit, at);
    }
    for (std::size_t i = 0u; i < model.edges.size(); ++i) {
        work.spend(256u);
        const auto p = projections[i];
        if (skip[i] || p.from == p.to) continue;
        std::string_view direction = model.direction;
        for (int at = p.owner; at >= 0; at = parent(at))
            if (!model.groups[static_cast<std::size_t>(at)].direction.empty()) {
                direction = model.groups[static_cast<std::size_t>(at)].direction;
                break;
            }
        const auto& a = entities[static_cast<std::size_t>(p.from)];
        const auto& b = entities[static_cast<std::size_t>(p.to)];
        const int ax = a.left + a.right, bx = b.left + b.right, ay = a.top + a.bottom,
                  by = b.top + b.bottom;
        if ((direction == "LR" && ax >= bx) || (direction == "RL" && ax <= bx) ||
            ((direction == "TD" || direction == "TB") && ay >= by) ||
            (direction == "BT" && ay <= by))
            return false;
    }
    return true;
}

inline Result geometry_impl(const Grid& grid, const Model& model, OracleWork& work,
                            std::string_view token, Limits limits) {
    work.spend(32768u);
    if (limits.path_cells > 2048u || limits.strands > 14u) return inconclusive(Reason::Limit, true);
    const auto n = model.nodes.size(), g = model.groups.size(), e = model.edges.size();
    const auto valid_direction = [](std::string_view value) {
        return value == "TD" || value == "TB" || value == "BT" || value == "LR" || value == "RL";
    };
    if (n == 0u || n > 11u || g > 2u || e > 14u || !valid_direction(model.direction))
        return inconclusive(Reason::Domain);
    std::set<std::string> labels;
    std::vector<int> owners(n, -1);
    const auto label_valid = [&](const std::string& value) {
        work.product(value.size(), 16u);
        if (value.empty() || value.size() > 64u) return false;
        for (const unsigned char c : value)
            if (c < 0x20u || c > 0x7eu) return false;
        return labels.insert(value).second;
    };
    for (const auto& node : model.nodes)
        if (node.shape != Shape::Rectangle || !label_valid(node.label))
            return inconclusive(Reason::Domain);
    for (const auto& group : model.groups)
        if (!label_valid(group.label) ||
            (!group.direction.empty() && !valid_direction(group.direction)) || group.parent < -1 ||
            group.parent >= static_cast<int>(g))
            return inconclusive(Reason::Domain);
    for (std::size_t at = 0u; at < g; ++at) {
        const auto& group = model.groups[at];
        int depth = 0;
        for (int p = static_cast<int>(at); p >= 0;
             p = model.groups[static_cast<std::size_t>(p)].parent)
            if (++depth > static_cast<int>(g)) return inconclusive(Reason::Domain);
        for (const int member : group.nodes) {
            work.spend(128u);
            if (member < 0 || static_cast<std::size_t>(member) >= n ||
                owners[static_cast<std::size_t>(member)] >= 0)
                return inconclusive(Reason::Domain);
            owners[static_cast<std::size_t>(member)] = static_cast<int>(at);
        }
    }
    std::array<int, 11> expected_degree{};
    std::size_t expected_heads = 0u;
    for (const auto& edge : model.edges) {
        work.spend(256u);
        if (edge.from < 0 || edge.to < 0 || static_cast<std::size_t>(edge.from) >= n ||
            static_cast<std::size_t>(edge.to) >= n || edge.from == edge.to || !edge.label.empty() ||
            edge.style != "solid" || edge.start)
            return inconclusive(Reason::Domain);
        ++expected_degree[static_cast<std::size_t>(edge.from)];
        ++expected_degree[static_cast<std::size_t>(edge.to)];
        expected_heads += edge.end;
    }
    if (grid.width <= 0 || grid.height <= 0 || grid.width > 240 || grid.height > 1040)
        return inconclusive(Reason::Limit, true);
    if (grid.cells.size() != static_cast<std::size_t>(grid.width * grid.height) || token.empty() ||
        token.size() > 240u)
        return inconclusive(Reason::Domain);
    work.product(token.size(), 8u);
    for (const unsigned char c : token)
        if (c < 0x21u || c > 0x7eu) return inconclusive(Reason::Domain);
    const auto count = grid.cells.size();
    work.product(count, 32u);
    work.spend(4096u);
    int following = -1, last_body = -1;
    std::size_t following_count = 0u;
    for (int y = 0; y < grid.height; ++y) {
        bool exact = static_cast<std::size_t>(grid.width) >= token.size(), ink = false;
        for (int x = 0; x < grid.width; ++x) {
            const auto cell = grid.at({x, y});
            work.product(cell.size(), 4u);
            const auto expected = static_cast<std::size_t>(x) < token.size()
                                      ? token.substr(static_cast<std::size_t>(x), 1u)
                                      : std::string_view{" "};
            if (cell != expected) exact = false;
            if (!cell.empty() && cell != " ") ink = true;
        }
        if (exact) {
            following = y;
            ++following_count;
        } else if (ink)
            last_body = y;
    }
    if (following_count != 1u || following <= last_body) return mismatch(Reason::Footer);
    // Prepay zeroed masks, port lookup, label ownership and complete-grid passes.
    work.product(count, 96u);
    work.spend(16384u);
    auto frames = closed_frames(grid, work);
    if (frames.size() > 128u) return inconclusive(Reason::Limit, true);
    work.product(frames.size(), frames.size() * (n + g) * 32u + 64u);
    std::vector<ShapeFrame> entities;
    entities.reserve(n + g);
    std::set<int> assigned;
    for (std::size_t at = 0u; at < n + g; ++at) {
        const auto& label = at < n ? model.nodes[at].label : model.groups[at - n].label;
        auto options = frame_candidates(grid, frames, label, Shape::Rectangle, work);
        if (options.empty()) return mismatch(Reason::Identity);
        if (options.size() != 1u) return inconclusive(Reason::Identity);
        if (!assigned.insert(options[0]).second) return mismatch(Reason::Identity);
        const auto frame = frames[static_cast<std::size_t>(options[0])];
        if (at < n && !complete_node_content(grid, frame, label, work))
            return mismatch(Reason::Ink);
        entities.push_back(frame);
    }
    const auto ancestor = [&](int a, int b) {
        for (int p = a; p >= 0; p = model.groups[static_cast<std::size_t>(p)].parent)
            if (p == b) return true;
        return false;
    };
    for (std::size_t a = 0u; a < g; ++a) {
        const auto& frame = entities[n + a];
        const int parent = model.groups[a].parent;
        if (parent >= 0 && !frame.inside(entities[n + static_cast<std::size_t>(parent)]))
            return mismatch(Reason::Ownership);
        for (std::size_t b = 0u; b < a; ++b) {
            work.spend(128u);
            const auto& other = entities[n + b];
            if (!ancestor(static_cast<int>(a), static_cast<int>(b)) &&
                !ancestor(static_cast<int>(b), static_cast<int>(a)) &&
                !(frame.right < other.left || other.right < frame.left ||
                  frame.bottom < other.top || other.bottom < frame.top))
                return mismatch(Reason::Ownership);
        }
    }
    for (std::size_t a = 0u; a < n; ++a) {
        for (std::size_t b = 0u; b < a; ++b) {
            work.spend(128u);
            const auto& x = entities[a];
            const auto& y = entities[b];
            if (!(x.right < y.left || y.right < x.left || x.bottom < y.top || y.bottom < x.top))
                return mismatch(Reason::Ownership);
        }
        for (std::size_t b = 0u; b < g; ++b) {
            work.spend(128u);
            const bool member = ancestor(owners[a], static_cast<int>(b));
            const auto& node = entities[a];
            const auto& group = entities[n + b];
            if (member ? !node.inside(group)
                       : !(node.right < group.left || group.right < node.left ||
                           node.bottom < group.top || group.bottom < node.top))
                return mismatch(Reason::Ownership);
        }
    }
    if (!directions(model, entities, owners, work)) return mismatch(Reason::Direction);
    std::vector<unsigned char> frame_bits(count), route_bits(count), claimed(count),
        trace_seen(count);
    std::vector<int> node_at(count, -1), terminal_at(count, -1), border_at(count, -1);
    std::vector<bool> label_at(count);
    for (std::size_t at = 0u; at < n + g; ++at) {
        const auto& frame = entities[at];
        const auto& text = at < n ? model.nodes[at].label : model.groups[at - n].label;
        work.product(static_cast<std::size_t>((frame.right - frame.left + 1) *
                                              (frame.bottom - frame.top + 1)),
                     32u);
        for (int y = frame.top; y <= frame.bottom; ++y)
            for (int x = frame.left; x <= frame.right; ++x) {
                const Point p{x, y};
                const auto index = static_cast<std::size_t>(grid.index(p));
                if (frame.boundary(p))
                    frame_bits[index] |= static_cast<unsigned char>(frame.perimeter_bits(p));
                if (at < n && frame.contains(p)) node_at[index] = static_cast<int>(at);
            }
        auto blocks = label_blocks(grid, text, frame, work);
        std::size_t direct = 0u;
        for (const auto& block : blocks) {
            bool own = true;
            for (const auto start : block.starts)
                for (std::size_t other = 0u; other < entities.size(); ++other) {
                    work.spend(8u);
                    if (other != at && entities[other].inside(frame) &&
                        entities[other].interior(start))
                        own = false;
                }
            if (own) {
                ++direct;
                work.product(text.size(), 16u);
                claim_label(grid, text, block, label_at);
            }
        }
        if (direct != 1u) return inconclusive(Reason::Identity);
    }
    std::array<Port, 28> ports{};
    std::size_t port_count = 0u, head_count = 0u;
    std::array<int, 11> actual_degree{};
    const auto add_port = [&](Port port) -> bool {
        if (port_count >= ports.size()) return false;
        const auto index = static_cast<std::size_t>(grid.index(port.terminal)),
                   border = static_cast<std::size_t>(grid.index(port.border));
        if (terminal_at[index] >= 0 || border_at[border] >= 0) return false;
        terminal_at[index] = static_cast<int>(port_count);
        border_at[border] = static_cast<int>(port_count);
        ports[port_count++] = port;
        ++actual_degree[static_cast<std::size_t>(port.node)];
        return true;
    };
    for (int y = 0; y < following; ++y)
        for (int x = 0; x < grid.width; ++x) {
            work.spend(32u);
            const Point p{x, y};
            const auto index = static_cast<std::size_t>(grid.index(p));
            const auto cell = glyph(grid.at(p));
            if (cell.stroke != 0 || grid.at(p) == "╭" || grid.at(p) == "╮" || grid.at(p) == "╯" ||
                grid.at(p) == "╰")
                return mismatch(Reason::Ink);
            if (node_at[index] >= 0) {
                const auto node = static_cast<std::size_t>(node_at[index]);
                if (entities[node].boundary(p)) {
                    const int extra = cell.bits & ~frame_bits[index];
                    if (extra && (!one_bit(extra) || !entities[node].legal_port(p, extra) ||
                                  !add_port({static_cast<int>(node), p, p, extra, false})))
                        return mismatch(Reason::Port);
                }
                continue;
            }
            if (cell.arrow >= 0) {
                ++head_count;
                const int face = kBits[static_cast<std::size_t>(cell.arrow)];
                const auto border = step(p, face);
                if (!grid.has(border)) return mismatch(Reason::Marker);
                const auto target = node_at[static_cast<std::size_t>(grid.index(border))];
                if (target < 0 ||
                    !entities[static_cast<std::size_t>(target)].legal_port(border, reverse(face)) ||
                    frame_bits[index] != 0u || !add_port({target, border, p, reverse(face), true}))
                    return mismatch(Reason::Marker);
                continue;
            }
            const int mask = cell.bits & ~frame_bits[index];
            route_bits[index] = static_cast<unsigned char>(mask);
            if (frame_bits[index] && mask &&
                (!opposite_pair(frame_bits[index]) || !opposite_pair(mask) || cell.bits != 15))
                return mismatch(Reason::Route);
            if (mask && !opposite_pair(mask) && std::popcount(static_cast<unsigned>(mask)) != 2 &&
                mask != 15)
                return mismatch(Reason::Route);
            if (!mask && !frame_bits[index] && !label_at[index] && !grid.at(p).empty() &&
                grid.at(p) != " ")
                return mismatch(Reason::Ink);
            if (mask && label_at[index]) return mismatch(Reason::Ink);
        }
    if (head_count != expected_heads) return mismatch(Reason::Marker);
    if (port_count != 2u * e) return mismatch(Reason::Port);
    for (std::size_t at = 0u; at < n; ++at)
        if (actual_degree[at] != expected_degree[at]) return mismatch(Reason::Port);
    std::array<bool, 28> consumed{};
    std::array<EdgeKey, 14> actual{};
    std::size_t strands = 0u;
    for (std::size_t start = 0u; start < port_count; ++start) {
        if (consumed[start] || ports[start].head) continue;
        if (strands >= limits.strands) return inconclusive(Reason::Limit, true);
        const auto& source = ports[start];
        std::size_t length = 1u;
        if (length > limits.path_cells) return inconclusive(Reason::Limit, true);
        work.product(count, 2u);
        std::ranges::fill(trace_seen, 0u);
        auto current = step(source.border, source.outward);
        int incoming = reverse(source.outward);
        const auto source_index = static_cast<std::size_t>(grid.index(source.border));
        if (claimed[source_index] & source.outward) return mismatch(Reason::Route);
        claimed[source_index] |= static_cast<unsigned char>(source.outward);
        while (true) {
            work.spend(256u);
            if (++length > limits.path_cells) return inconclusive(Reason::Limit, true);
            if (!grid.has(current) || current.y >= following) return mismatch(Reason::Route);
            const auto index = static_cast<std::size_t>(grid.index(current));
            const int terminal = terminal_at[index];
            if (terminal >= 0) {
                const auto target = static_cast<std::size_t>(terminal);
                const auto& port = ports[target];
                if (target == start || consumed[target] || incoming != port.outward)
                    return mismatch(Reason::Port);
                if (port.head) {
                    if (++length > limits.path_cells) return inconclusive(Reason::Limit, true);
                    const auto arrow = glyph(grid.at(current));
                    if (arrow.arrow < 0 || arrow.bits != incoming) return mismatch(Reason::Marker);
                    if (claimed[index]) return mismatch(Reason::Route);
                    claimed[index] = static_cast<unsigned char>(incoming);
                } else {
                    if (claimed[index] & port.outward) return mismatch(Reason::Route);
                    claimed[index] |= static_cast<unsigned char>(port.outward);
                }
                consumed[start] = true;
                consumed[target] = true;
                actual[strands++] = key(source.node, port.node, port.head);
                break;
            }
            if (node_at[index] >= 0 || label_at[index] || glyph(grid.at(current)).arrow >= 0)
                return mismatch(Reason::Route);
            const int mask = route_bits[index];
            if (!(mask & incoming)) return mismatch(Reason::Route);
            if (trace_seen[index] != 0u) return mismatch(Reason::Route);
            trace_seen[index] |= static_cast<unsigned char>(incoming);
            const int outgoing = mask == 15 ? reverse(incoming) : other_end(mask, incoming);
            if (!one_bit(outgoing)) return mismatch(Reason::Route);
            const int arms = incoming | outgoing;
            if (claimed[index] & arms) return mismatch(Reason::Route);
            claimed[index] |= static_cast<unsigned char>(arms);
            current = step(current, outgoing);
            incoming = reverse(outgoing);
        }
    }
    for (std::size_t at = 0u; at < port_count; ++at)
        if (!consumed[at]) return mismatch(Reason::Port);
    std::array<bool, 14> matched{};
    for (std::size_t at = 0u; at < e; ++at) {
        work.spend(512u);
        const auto& edge = model.edges[at];
        const auto expected = key(edge.from, edge.to, edge.end);
        bool found = false;
        for (std::size_t candidate = 0u; candidate < strands; ++candidate)
            if (!matched[candidate] && actual[candidate] == expected) {
                matched[candidate] = true;
                found = true;
                break;
            }
        if (!found) return mismatch(Reason::Multiplicity);
    }
    if (strands != e) return mismatch(Reason::Multiplicity);
    for (int y = 0; y < following; ++y)
        for (int x = 0; x < grid.width; ++x) {
            work.spend(32u);
            const Point p{x, y};
            const auto at = static_cast<std::size_t>(grid.index(p));
            const auto value = glyph(grid.at(p));
            if (node_at[at] >= 0 && entities[static_cast<std::size_t>(node_at[at])].interior(p))
                continue;
            if (label_at[at]) {
                if (value.bits || value.arrow >= 0) return mismatch(Reason::Conservation);
                continue;
            }
            if (value.bits != (frame_bits[at] | claimed[at])) return mismatch(Reason::Conservation);
            if (value.bits == 0 && !grid.at(p).empty() && grid.at(p) != " ")
                return mismatch(Reason::Ink);
        }
    return {Outcome::Match, Reason::None, false, strands};
}

inline Result geometry(const Grid& grid, const Model& model, OracleWork& work,
                       std::string_view token = kSentinel, Limits limits = {}) {
    try {
        return geometry_impl(grid, model, work, token, limits);
    } catch (const FlowContourLimit&) {
        return inconclusive(Reason::Limit, true);
    } catch (const std::bad_alloc&) {
        return inconclusive(Reason::Work, true);
    } catch (const std::exception&) {
        return inconclusive(work.remaining == 0u ? Reason::Work : Reason::Internal,
                            work.remaining == 0u);
    }
}
}  // namespace loom::test::mermaid_flow::strands
