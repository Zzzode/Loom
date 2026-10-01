# ─── loom_cli: CLI Transports ───────────────────────────────────────────────────
add_library(loom_cli)
target_sources(loom_cli
    PUBLIC FILE_SET CXX_MODULES FILES
        cli/ccr_client.cppm
        cli/sse_transport.cppm
        cli/update.cppm
        cli/websocket_transport.cppm
)
target_link_libraries(loom_cli
    PUBLIC
        loom_utils
        yyjson
        uv_a
        httplib::httplib
        # SSETransport performs a real TLS handshake for https:// streams.
        OpenSSL::SSL
        OpenSSL::Crypto
)
