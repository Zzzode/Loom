// Complete-source terminal Model reviewed by agent:default_config_source_review.
// The description envelope is metadata; no browser/theme equivalence is claimed.
#pragma once
#include "mermaid_flow400_authority.hpp"

namespace loom::test::mermaid_flow {
inline constexpr std::string_view kDefaultConfigPath =
    "conf-and-directives/pr-check/no-config-use-default.mmd";
inline constexpr std::string_view kDefaultConfigSha =
    "6f56685a482928f690a4920225dd1e89047586c3d277664cd68e4e84b4b9bb83";

inline Model default_config_model(std::string_view source) {
    flow400_require(digest(source) == kDefaultConfigSha,
                    "DefaultConfig complete original SHA mismatch");
    Model model;
    model.direction = "TB";
    model.nodes = {{"A", "Default", Shape::Rounded},
                   {"B", "Another", Shape::Parallelogram},
                   {"C", "End", Shape::Rectangle}};
    model.edges = {{0, 1, "", "solid", false, true}, {0, 2, "", "solid", false, true}};
    model.groups = {{"section", "section", "", -1, {1, 2}}};
    return model;
}
}  // namespace loom::test::mermaid_flow
