/// @file test_sdk_convergence.cpp
/// @brief Compile-time guards that the SDK island's CONVERGE aliases point at
///        the canonical types (RFC 0001 phase 2, design §4.2).
///
/// If a future change re-introduces a duplicate enum/struct in the island
/// instead of aliasing the canonical type, these static_asserts fail at
/// compile time — catching the drift before it reaches review.
#include <type_traits>

import loom.sdk.core_schemas;
import loom.sdk.runtime_types;

// Canonical type modules (for the right-hand side of each is_same_v).
import loom.config.settings;
import loom.tools.agent_runtime;
import loom.model.effort;

// ── core_schemas CONVERGE aliases ───────────────────────────────────────────

static_assert(
    std::is_same_v<loom::sdk::core_schemas::ConfigScope,
                   loom::config::SettingsScope>,
    "loom::sdk::core_schemas::ConfigScope must alias loom::config::SettingsScope");

static_assert(
    std::is_same_v<loom::sdk::core_schemas::SettingSource,
                   loom::config::SettingsScope>,
    "loom::sdk::core_schemas::SettingSource must alias loom::config::SettingsScope");

static_assert(
    std::is_same_v<loom::sdk::core_schemas::AgentDefinition,
                   loom::tools::agent_runtime::AgentDefinition>,
    "loom::sdk::core_schemas::AgentDefinition must alias "
    "loom::tools::agent_runtime::AgentDefinition");

// ── runtime_types CONVERGE aliases ──────────────────────────────────────────

static_assert(
    std::is_same_v<loom::sdk::runtime::EffortLevel,
                   loom::utils::EffortLevel>,
    "loom::sdk::runtime::EffortLevel must alias loom::utils::EffortLevel");

// ── core_types re-exports resolve through the aliases ───────────────────────

import loom.sdk.core_types;

static_assert(
    std::is_same_v<loom::sdk::core_types::ConfigScope,
                   loom::config::SettingsScope>,
    "loom::sdk::core_types::ConfigScope must resolve to loom::config::SettingsScope");

static_assert(
    std::is_same_v<loom::sdk::core_types::SettingSource,
                   loom::config::SettingsScope>,
    "loom::sdk::core_types::SettingSource must resolve to "
    "loom::config::SettingsScope");

static_assert(
    std::is_same_v<loom::sdk::core_types::AgentDefinition,
                   loom::tools::agent_runtime::AgentDefinition>,
    "loom::sdk::core_types::AgentDefinition must resolve to "
    "loom::tools::agent_runtime::AgentDefinition");

// All checks are compile-time (static_assert). The test executable links
// gtest_main so the linker is satisfied; no runtime tests are needed.
