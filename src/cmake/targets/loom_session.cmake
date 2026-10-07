# ─── loom_session: Session Management ──────────────────────────────────────────
add_library(loom_session)
target_sources(loom_session
    PUBLIC FILE_SET CXX_MODULES FILES
        session/history.cppm
        session/storage.cppm
        # RFC-0001 Phase D B5d: moved from loom_utils (src/utils/session/).
        # list_sessions is JSONL-based (loom::utils free functions used by
        # the /insights command); loom_session links loom_utils PUBLIC, so
        # deps are satisfied.
        session/list_sessions.cppm
)
target_link_libraries(loom_session PUBLIC loom_utils loom_state loom_config yyjson)
