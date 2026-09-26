// C++23 Module: runtime backend installation for the orchestration layer.
//
// RFC 0001 Phase B: cc_orchestration is the rank-9 layer that wires concrete
// lower-ranked services (cc_services, cc_skills, ...) into the callback
// ports owned by the tools layer. This batch installs ONE backend: the
// cc.services.image-backed image codec (cc.tools.image_codec.port). Later
// batches add the skill-loader executor and the lifted runtime tool
// backends in this same module.
module;

#include <cstdint>

export module cc.orchestration.runtime_backends;

import std;

import cc.services.image;
import cc.tools.image_codec.port;

export namespace cc::orchestration {

/// Build the production image codec: a thin forwarder over
/// cc::services::image::ImageService. Exposed (not just used by the
/// installer) so test fixtures can reinstall the real codec after clearing
/// the process-global slot.
[[nodiscard]] inline cc::tools::image_codec::Codec make_image_codec() {
    namespace image_codec = cc::tools::image_codec;
    namespace svc_image = cc::services::image;

    image_codec::Codec result;

    result.get_info = [](const std::filesystem::path& path)
        -> std::expected<image_codec::ImageInfo, std::string> {
        auto info = svc_image::ImageService::get_info(path);
        if (!info) {
            return std::unexpected(info.error().message);
        }
        return image_codec::ImageInfo{
            .width = info->width,
            .height = info->height,
            .size_bytes = info->size_bytes,
            .mime = std::string(svc_image::format_to_mime(info->format)),
            .summary = info->summary(),
        };
    };

    result.to_base64 = [](std::span<const std::uint8_t> data) -> std::string {
        return svc_image::ImageService::to_base64(data);
    };

    result.from_base64 = [](std::string_view encoded)
        -> std::expected<std::vector<std::uint8_t>, std::string> {
        auto decoded = svc_image::ImageService::from_base64(encoded);
        if (!decoded) {
            return std::unexpected(decoded.error().message);
        }
        return std::move(*decoded);
    };

    return result;
}

/// Install every orchestration-owned runtime backend exactly once. The
/// std::call_once guard makes the call safe from the per-request server
/// route threads; repeat calls after the first are no-ops. This batch wires
/// the image codec only — the skill executor and lifted runtime tool
/// backends arrive in later Phase B batches.
inline void install_runtime_backends() {
    static std::once_flag once;
    std::call_once(once, [] {
        cc::tools::image_codec::set_codec(make_image_codec());
    });
}

} // namespace cc::orchestration
