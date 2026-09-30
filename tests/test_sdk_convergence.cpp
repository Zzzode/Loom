/// @file test_sdk_convergence.cpp
/// @brief Compile-time guards that the SDK island's CONVERGE aliases point at
///        the canonical types (RFC 0001 phase 2, design §4.2).
///
/// If a future change re-introduces a duplicate enum/struct in the island
/// instead of aliasing the canonical type, these static_asserts fail at
/// compile time — catching the drift before it reaches review.
#include <type_traits>

import cc.sdk.core_schemas;
import cc.sdk.runtime_types;

// Canonical type modules (for the right-hand side of each is_same_v).
import cc.config.settings;
import cc.tools.agent_runtime;
import cc.model.effort;

// ── core_schemas CONVERGE aliases ───────────────────────────────────────────

static_assert(
    std::is_same_v<cc::sdk::core_schemas::ConfigScope,
                   cc::config::SettingsScope>,
    "cc::sdk::core_schemas::ConfigScope must alias cc::config::SettingsScope");

static_assert(
    std::is_same_v<cc::sdk::core_schemas::SettingSource,
                   cc::config::SettingsScope>,
    "cc::sdk::core_schemas::SettingSource must alias cc::config::SettingsScope");

static_assert(
    std::is_same_v<cc::sdk::core_schemas::AgentDefinition,
                   cc::tools::agent_runtime::AgentDefinition>,
    "cc::sdk::core_schemas::AgentDefinition must alias "
    "cc::tools::agent_runtime::AgentDefinition");

// ── runtime_types CONVERGE aliases ──────────────────────────────────────────

static_assert(
    std::is_same_v<cc::sdk::runtime::EffortLevel,
                   cc::utils::EffortLevel>,
    "cc::sdk::runtime::EffortLevel must alias cc::utils::EffortLevel");

// ── core_types re-exports resolve through the aliases ───────────────────────

import cc.sdk.core_types;

static_assert(
    std::is_same_v<cc::sdk::core_types::ConfigScope,
                   cc::config::SettingsScope>,
    "cc::sdk::core_types::ConfigScope must resolve to cc::config::SettingsScope");

static_assert(
    std::is_same_v<cc::sdk::core_types::SettingSource,
                   cc::config::SettingsScope>,
    "cc::sdk::core_types::SettingSource must resolve to "
    "cc::config::SettingsScope");

static_assert(
    std::is_same_v<cc::sdk::core_types::AgentDefinition,
                   cc::tools::agent_runtime::AgentDefinition>,
    "cc::sdk::core_types::AgentDefinition must resolve to "
    "cc::tools::agent_runtime::AgentDefinition");

// All checks are compile-time (static_assert). The test executable links
// gtest_main so the linker is satisfied; no runtime tests are needed.
