/// @file image_codec_port.cppm
/// @brief Image codec port — orchestration-owned callback leaf.
///
/// File/image tools need image metadata and base64 codec services that are
/// implemented in cc_services (cc::services::image::ImageService) and wired by
/// cc_orchestration, but cc_tools cannot depend on cc_orchestration (rank 8 ->
/// rank 9 would be an upward edge). The concrete backend is installed once at
/// process startup via cc::orchestration::install_runtime_backends(); tool
/// code acquires it through codec() and fails closed when no deployment
/// installed one (hermetic test binaries).
module;

#include <cstddef>
#include <cstdint>

export module cc.tools.image_codec.port;

import std;

export namespace cc::tools::image_codec {

/// Image metadata returned by the installed codec. Mirrors the fields of
/// cc::services::image::ImageInfo that callers consume, with the format
/// already resolved to a MIME type.
struct ImageInfo {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::size_t size_bytes = 0;
    std::string mime;
    std::string summary;
};

using GetInfoFn = std::function<
    std::expected<ImageInfo, std::string>(const std::filesystem::path&)>;
using ToBase64Fn = std::function<
    std::string(std::span<const std::uint8_t>)>;
using FromBase64Fn = std::function<
    std::expected<std::vector<std::uint8_t>, std::string>(std::string_view)>;

/// Installed image codec. Truthy only when all three entry points are set
/// (the production installer wires them atomically together).
struct Codec {
    GetInfoFn get_info;
    ToBase64Fn to_base64;
    FromBase64Fn from_base64;

    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(get_info) &&
               static_cast<bool>(to_base64) &&
               static_cast<bool>(from_base64);
    }
};

namespace detail {

/// Single function-local static storage; avoids header-order static
/// initialization issues (same anchor pattern as the file-access hook port).
[[nodiscard]] inline Codec& image_codec_storage() {
    static Codec instance;
    return instance;
}

} // namespace detail

/// Install the process-wide image codec. Called once from the orchestration
/// installer; passing a default-constructed Codec clears it.
inline void set_codec(Codec codec) {
    detail::image_codec_storage() = std::move(codec);
}

/// Reset the process-wide codec to the unset state. Test fixtures use this
/// so cases stay hermetic; production never clears.
inline void clear_codec() {
    detail::image_codec_storage() = Codec{};
}

/// Acquire the installed codec. Returns a reference to the function-local
/// static; check it with operator bool before invoking an entry point.
[[nodiscard]] inline const Codec& codec() {
    return detail::image_codec_storage();
}

[[nodiscard]] inline bool has_codec() {
    return static_cast<bool>(detail::image_codec_storage());
}

} // namespace cc::tools::image_codec
