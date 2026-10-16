# Existing wire serializers/parsers shared by the query and delegated loops.
# Keep exported module names and paths stable; this target has no engine deps.
add_library(loom_wire)
target_sources(loom_wire PUBLIC FILE_SET CXX_MODULES FILES
    query/wire_protocol.cppm
    query/wire_messages.cppm
    query/wire_openai.cppm
)
target_link_libraries(loom_wire PUBLIC loom_tools loom_types loom_utils)
