// API provider backends
module;

export module loom.services.api.models;

import std;


export namespace loom::services::api {

// API provider backends
enum class Provider {
    Messages,
    Bedrock,
    Vertex,
};

// Convert provider to display string
constexpr std::string_view provider_name(Provider p) {
    switch (p) {
        case Provider::Messages: return "messages";
        case Provider::Bedrock: return "bedrock";
        case Provider::Vertex: return "vertex";
    }
    return "unknown";
}

} // namespace loom::services::api
