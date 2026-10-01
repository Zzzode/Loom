/// @file swarm.cpp
/// @brief Minimal worker identity types shared by the mailbox router and the
/// task graph. The toy in-process/tmux backends, SwarmManager and permission
/// cache that previously lived here were unused dead code (the real backends
/// live in loom.teams.swarm.backends); they were removed in the D-reconn stage.
module;

#include <cstdint>

export module loom.coordinator.swarm;

import std;

import loom.types.types;

export namespace loom::core {

/// Strong ID for worker agents
struct WorkerIdTag {};
using WorkerId = StrongId<WorkerIdTag>;

} // namespace loom::core
