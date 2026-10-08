# ─── loom_query: Query Engine ───────────────────────────────────────────────────
add_library(loom_query)
target_sources(loom_query
    PUBLIC FILE_SET CXX_MODULES FILES
        query/wire_protocol.cppm
        query/wire_messages.cppm
        query/wire_openai.cppm
        query/query_engine.cppm
        # RFC 0001 cc-sdk phase 3 (§2.1): the assemble-only engine recipe
        # extracted from the server route, so both loom.server (rank 13) and
        # loom.sdk.harness (rank 16) wrap the same assembly without an upward
        # edge. Bodies in query_assembly.cpp (impl unit), not the BMI.
        query/query_assembly.cppm
        # RFC 0004: StreamEvent/ContentBlock/Message → JSONL fixture serializer
        query/fixture_serializer.cppm
)
# RFC 0001 Phase C batch 5 — query_engine module implementation units. Never
# add these to the FILE_SET CXX_MODULES list above: they are module impl
# units (`module loom.query.query_engine;`), not interface units. In
# particular query_engine_http.cpp is the ONLY TU that textually includes
# <httplib.h>, keeping the third-party closure out of the interface BMI.
target_sources(loom_query
    PRIVATE
        query/query_engine_ctor.cpp
        query/query_engine_system_prompt.cpp
        query/query_engine_loop.cpp
        query/query_engine_http.cpp
        query/query_engine_wire.cpp
        query/query_engine_json.cpp
        query/query_engine_tools.cpp
        query/query_engine_conversation.cpp
        query/query_engine_compaction.cpp
        query/query_engine_util.cpp
        # loom.query.assembly implementation unit (PIMPL special members +
        # resolve_engine_config + assemble bodies).
        query/query_assembly.cpp
        # RFC 0004: fixture serializer implementation unit
        query/fixture_serializer.cpp
)
# RFC 0001 cc-sdk phase 3 (§2.1, §5.6): loom_orchestration is the only
# genuinely new PUBLIC dep — loom_tools/loom_hooks/loom_session/loom_memdir/
# loom_services are already linked PUBLICly via loom_tools.cmake. The assembly
# needs loom_orchestration for install_runtime_backends,
# make_missing_tool_backend, and the collect_mcp_* providers. No cycle:
# loom.orchestration does not import loom.query.
target_link_libraries(loom_query PUBLIC loom_utils loom_state loom_config CURL::libcurl loom_orchestration)
