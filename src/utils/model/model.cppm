
export module loom.model.model;

import std;

export namespace loom::utils {

namespace detail {
    // Module-level state for current model
    inline std::string& current_model_ref() {
        static std::string model;
        return model;
    }
} // namespace detail

std::string get_current_model() {
    return detail::current_model_ref();
}

bool set_current_model(std::string_view model_id) {
    detail::current_model_ref() = std::string(model_id);
    return true;
}

bool validate_model_id(std::string_view model_id) {
    return !model_id.empty();
}

std::string get_model_display_name(std::string_view model_id) {
    return std::string(model_id);
}

} // namespace loom::utils
