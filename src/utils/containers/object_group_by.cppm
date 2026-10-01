module;

#include <cstddef>

export module loom.containers.object_group_by;

import std;

export namespace loom::utils::object_group_by {

template <typename T, typename K, typename Selector>
[[nodiscard]] inline std::map<K, std::vector<T>> object_group_by(const std::vector<T>& items, Selector key_selector) {
    std::map<K, std::vector<T>> result;
    std::size_t index = 0;
    for (const auto& item : items) {
        result[key_selector(item, index++)].push_back(item);
    }
    return result;
}

} // namespace loom::utils::object_group_by
