// Implementation unit for loom.orchestration.runtime_backends — the c13b
// structured 'config' runtime tool backend. A fresh ConfigManager is built
// PER CALL (path/env resolution happens at call time) and every load is
// quiet so the in-process TUI never paints a §A stderr diagnostic. Outputs
// are compact JSON text, never markdown; credential/endpoint values are
// surfaced as presence-only objects.
module;

module loom.orchestration.runtime_backends;

import std;

import loom.config.config;
import loom.types.tool_types;
import loom.tools.runtime_registry;
import loom.serdes.json;

namespace loom::orchestration::detail {

using loom::core::ConfigManager;
using loom::core::LoadOptions;
using loom::core::Result;
using loom::core::ToolInput;
using loom::core::ToolResult;

namespace json = loom::utils::json;

namespace {

[[nodiscard]] std::string make_list_payload(const ConfigManager& manager,
                                            bool loaded,
                                            std::string_view load_warning) {
    json::JsonMutDoc doc;
    auto out = doc.object();
    out.set("action", "list");
    out.add("specs", doc.raw_json(ConfigManager::serialize_user_setting_specs_json()));
    out.set("path", manager.user_config_path().string());
    out.set("user_file_valid", !manager.user_tier_unparseable());
    if (!loaded) out.set("load_warning", load_warning);
    doc.set_root(out);
    return doc.to_string();
}

} // namespace

[[nodiscard]] Result<ToolResult> config_backend(const ToolInput& input) {
    auto parsed = json::parse(input.json());
    if (!parsed || !parsed->root().is_obj()) {
        return ToolResult::error("config input must be a JSON object");
    }
    const auto root = parsed->root();
    const std::string action =
        loom::tools::detail::runtime_json_string(root, "action").value_or("get");
    const auto key = loom::tools::detail::runtime_json_string(root, "key");

    // Per-call manager: the user path and env engagement are resolved now.
    ConfigManager manager;
    const auto quiet = [] { return LoadOptions{.quiet = true}; };

    if (action == "list") {
        // Listing works even when a tier is hard-unparseable: the static
        // key sets are still returned, with the manager error attached.
        const auto loaded = manager.load(quiet());
        const std::string warning = loaded ? std::string{} : loaded.error().message;
        return ToolResult::success(
            make_list_payload(manager, loaded.has_value(), warning));
    }

    if (action == "get") {
        // Hard global/project load errors are surfaced to the caller
        // (recorded risk); user/local soft failures stay quiet.
        if (auto loaded = manager.load(quiet()); !loaded) {
            return ToolResult::error(loaded.error().message);
        }
        json::JsonMutDoc doc;
        auto out = doc.object();
        out.set("action", "get");
        out.set("path", manager.user_config_path().string());
        out.set("user_file_valid", !manager.user_tier_unparseable());
        if (!key) {
            out.add("settings",
                    doc.raw_json(manager.serialize_agent_settings_json()));
        } else if (auto presence = manager.agent_secret_presence_json(*key)) {
            out.add("setting", doc.raw_json(*presence));
        } else if (auto setting = manager.agent_setting_value_json(*key)) {
            out.add("setting", doc.raw_json(*setting));
        } else {
            return ToolResult::error(std::format(
                "Unknown configuration key '{}'. Run action=list to see the "
                "supported keys.", *key));
        }
        doc.set_root(out);
        return ToolResult::success(doc.to_string());
    }

    if (action == "set") {
        if (!key) {
            return ToolResult::error("config set requires a key");
        }
        if (!root.has("value")) {
            return ToolResult::error(
                "config set requires a value (use null to clear "
                "temperature or thinkingBudget)");
        }
        // set_user_setting validates, patches with salvage=ON, and performs
        // the quiet post-write reload itself.
        auto written = manager.set_user_setting(*key, root.get("value"));
        if (!written) {
            return ToolResult::error(written.error().message);
        }
        json::JsonMutDoc doc;
        auto out = doc.object();
        out.set("action", "set");
        out.set("key", *key);
        out.add("value", doc.raw_json(written->value_token));
        out.set("path", written->path.string());
        if (written->repaired) {
            out.set("repaired", *written->repaired);
        } else {
            out.add("repaired", out.make_null());
        }
        out.set("shadowed", written->shadowed);
        if (written->shadowed) {
            out.set("shadowed_by", written->shadowed_by);
        }
        if (written->reload_warning) {
            out.set("reload_warning", *written->reload_warning);
        }
        doc.set_root(out);
        return ToolResult::success(doc.to_string());
    }

    return ToolResult::error(std::format(
        "Unknown config action '{}' (expected get, set, or list)", action));
}

} // namespace loom::orchestration::detail
