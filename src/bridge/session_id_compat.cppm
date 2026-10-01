module;
#include <cctype>
#include <cstddef>
#include <cstdint>

export module loom.bridge.session_id_compat;

import std;

export namespace loom::bridge {

// Normalize a session ID to the current format (lowercase, no dashes variant)
std::string normalize_session_id(std::string_view id) {
    std::string normalized(id);

    // Remove any surrounding whitespace
    while (!normalized.empty() && (normalized.front() == ' ' || normalized.front() == '\t')) {
        normalized.erase(normalized.begin());
    }
    while (!normalized.empty() && (normalized.back() == ' ' || normalized.back() == '\t')) {
        normalized.pop_back();
    }

    // Convert to lowercase
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return normalized;
}

// Generate a new session ID in the current format
std::string generate_session_id() {
    // Current format: "ses_" + 32 random hex characters
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;

    std::ostringstream oss;
    oss << "ses_" << std::hex << std::setfill('0');
    oss << std::setw(16) << dist(gen);
    oss << std::setw(16) << dist(gen);

    return oss.str();
}

} // namespace loom::bridge
