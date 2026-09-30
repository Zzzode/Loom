# ─── cc_session: Session Management ──────────────────────────────────────────
add_library(cc_session)
target_sources(cc_session
    PUBLIC FILE_SET CXX_MODULES FILES
        session/history.cppm
        session/storage.cppm
        # RFC-0001 Phase D B5d: moved from cc_utils (src/utils/session/).
        # session_storage renames to cc.session.app_storage (D3: distinct from
        # cc.session.storage conversation persistence). Both import only
        # cc.utils.* / cc.serdes.json / cc.crypto.* + std; cc_session links
        # cc_utils PUBLIC, so deps are satisfied.
        session/list_sessions.cppm
        session/session_storage.cppm
)
target_link_libraries(cc_session PUBLIC cc_utils cc_state cc_config yyjson)
