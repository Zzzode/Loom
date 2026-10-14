module loom.ui.dialogs.system;

import std;

namespace loom::ui::dialogs::system {

bool DialogRendererRegistry::has_renderer_and_handler(DialogType type) const noexcept {
    const auto index = static_cast<std::size_t>(type);
    return index < renderers_.size() && index < event_handlers_.size()
        && renderers_[index] && event_handlers_[index];
}

} // namespace loom::ui::dialogs::system
