// swarm_helpers_shard.cpp — implementation unit for cc.teams.swarm.helpers
// (RFC-0001 B followup c16). Holds the body of
// WorkerPermissionGrants::shard_index_for so the FNV-1a hashing loop does
// not add an inline definition to the frozen inline-def count of the
// swarm_helpers interface unit. The selector itself is pure: one canonical
// grants path always maps to one shard mutex.
module;

module cc.teams.swarm.helpers;

import std;

namespace cc::utils::swarm_helpers {

std::size_t WorkerPermissionGrants::shard_index_for(
    std::string_view canonical_path) {
    constexpr std::uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
    constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
    std::uint64_t hash = kFnvOffsetBasis;
    for (unsigned char byte : canonical_path) {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= kFnvPrime;
    }
    return static_cast<std::size_t>(hash % kShardCount);
}

}  // namespace cc::utils::swarm_helpers
