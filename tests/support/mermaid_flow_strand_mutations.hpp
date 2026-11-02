// Decisive geometry negatives only. Inconclusive outcomes never earn credit.
#pragma once
#include "mermaid_flow_p0_strand_authority.hpp"

namespace loom::test::mermaid_flow::strands {
inline constexpr std::size_t kRequiredMutations = 7u;

struct MutationProof {
    std::size_t rejected = 0u;
    bool complete = false, resource = false, inconclusive = false;
};

inline MutationProof mutations(const Grid& good, const Model& model, OracleWork& work,
                               std::string_view token = kSentinel) {
    MutationProof proof;
    try {
        const auto baseline = geometry(good, model, work, token);
        if (baseline.outcome != Outcome::Match) {
            proof.resource = baseline.resource;
            proof.inconclusive = true;
            return proof;
        }
        const auto reject = [&](const Grid& pixels, const Model& expected) {
            const auto result = geometry(pixels, expected, work, token);
            if (result.outcome != Outcome::Mismatch) {
                proof.resource = result.resource;
                proof.inconclusive = true;
                return false;
            }
            ++proof.rejected;
            return true;
        };
        const auto semantic = [&](auto change) {
            flow400_copy_model(model, work);
            auto bad = model;
            change(bad);
            return reject(good, bad);
        };
        if (!semantic([](Model& bad) { bad.nodes[0].label = "absent_source_node"; })) return proof;
        if (!semantic([](Model& bad) {
                for (auto& group : bad.groups)
                    if (!group.nodes.empty()) {
                        group.nodes.erase(group.nodes.begin());
                        return;
                    }
                throw std::runtime_error("Flow strand membership mutation unavailable");
            }))
            return proof;
        if (!semantic([](Model& bad) { bad.edges[0].end = !bad.edges[0].end; })) return proof;
        if (!semantic([](Model& bad) { bad.edges.pop_back(); })) return proof;
        if (!semantic([](Model& bad) {
                bad.direction = bad.direction == "LR"   ? "RL"
                                : bad.direction == "RL" ? "LR"
                                : bad.direction == "BT" ? "TD"
                                                        : "BT";
            }))
            return proof;
        work.spend(16384u);
        const auto frames = closed_frames(good, work);
        work.product(frames.size(),
                     frames.size() * (model.nodes.size() + model.groups.size()) * 32u + 64u);
        std::vector<ShapeFrame> entities;
        entities.reserve(model.nodes.size() + model.groups.size());
        for (std::size_t at = 0u; at < model.nodes.size() + model.groups.size(); ++at) {
            const auto& label = at < model.nodes.size()
                                    ? model.nodes[at].label
                                    : model.groups[at - model.nodes.size()].label;
            const auto options = frame_candidates(good, frames, label, Shape::Rectangle, work);
            if (options.size() != 1u) {
                proof.inconclusive = true;
                return proof;
            }
            entities.push_back(frames[static_cast<std::size_t>(options[0])]);
        }
        const int following = flow400_following(good, work, token);
        work.product(good.cells.size(), entities.size() * 16u + 32u);
        std::optional<Point> blank, route;
        for (int y = 0; y < following && (!blank || !route); ++y)
            for (int x = 0; x < good.width && (!blank || !route); ++x) {
                const Point point{x, y};
                bool enclosed = false, boundary = false, node = false;
                for (std::size_t at = 0u; at < entities.size(); ++at) {
                    enclosed = enclosed || entities[at].contains(point);
                    boundary = boundary || entities[at].boundary(point);
                    node = node || (at < model.nodes.size() && entities[at].contains(point));
                }
                const auto value = glyph(good.at(point));
                if (!blank && !enclosed && good.at(point) == " ") blank = point;
                if (!route && !boundary && !node && value.arrow < 0 &&
                    std::popcount(static_cast<unsigned>(value.bits)) == 2)
                    route = point;
            }
        if (!blank || !route) {
            proof.inconclusive = true;
            return proof;
        }
        flow400_copy_grid(good, work);
        auto extra = good;
        extra.put(*blank, "§");
        if (!reject(extra, model)) return proof;
        flow400_copy_grid(good, work);
        auto broken = good;
        broken.put(*route, " ");
        if (!reject(broken, model)) return proof;
        proof.complete = proof.rejected == kRequiredMutations;
        return proof;
    } catch (const FlowContourLimit&) {
        proof.resource = true;
        proof.inconclusive = true;
        return proof;
    } catch (const std::bad_alloc&) {
        proof.resource = true;
        proof.inconclusive = true;
        return proof;
    } catch (const std::exception&) {
        proof.resource = work.remaining == 0u;
        proof.inconclusive = true;
        return proof;
    }
}
}  // namespace loom::test::mermaid_flow::strands
