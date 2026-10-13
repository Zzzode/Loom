// User-setting metadata lookup for loom.config.config.
module;

module loom.config.config;

import std;

namespace loom::core {

[[nodiscard]] const UserSettingSpec*
ConfigManager::find_user_setting(std::string_view dotted) noexcept {
    for (const auto& spec : user_setting_specs()) {
        if (spec.key == dotted) return &spec;
    }
    return nullptr;
}

} // namespace loom::core
