// LSP Types Module
module;

export module loom.services.lsp.types;

import std;

import loom.utils.error;

export namespace loom::services::lsp {

using loom::utils::Result;

// Scoped LSP server config
struct ScopedLspServerConfig {
    std::string command;
    std::vector<std::string> args;
    std::unordered_map<std::string, std::string> env;
    std::unordered_map<std::string, std::string> extension_to_language;
    std::optional<std::string> workspace_folder;
    std::string initialization_options_json = "{}";
};

// LSP Client config
struct LspClientConfig {
    std::string server_name;
    ScopedLspServerConfig config;
};

} // namespace loom::services::lsp
