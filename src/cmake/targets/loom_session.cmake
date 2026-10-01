# ─── loom_session: Session Management ──────────────────────────────────────────
add_library(loom_session)
target_sources(loom_session
    PUBLIC FILE_SET CXX_MODULES FILES
        session/history.cppm
        session/storage.cppm
        # RFC-0001 Phase D B5d: moved from loom_utils (src/utils/session/).
        # session_storage renames to cc.session.app_storage (D3: distinct from
        # cc.session.storage conversation persistence). Both import only
        # cc.utils.* / cc.serdes.json / cc.crypto.* + std; loom_session links
        # loom_utils PUBLIC, so deps are satisfied.
        session/list_sessions.cppm
        session/session_storage.cppm
)
target_link_libraries(loom_session PUBLIC loom_utils loom_state loom_config yyjson)
