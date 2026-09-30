# ─── cc_teams: Team/swarm runtime (RFC 0001 Phase D B6) ─────────────────────
# Created in B6: the six teams/swarm primaries (plus nine implementation
# units) moved out of cc_utils (src/utils/teams/, src/utils/swarm/) into
# their own target. Module names renamed cc.utils.* -> cc.teams.*. The
# modules import only cc_utils-resident modules (cc.serdes.json,
# cc.fs.atomic_replace, cc.process.bash.bash_execution) plus std, so
# cc_teams links cc_utils PUBLIC and there is no teams->tools/services edge.
add_library(cc_teams)
target_sources(cc_teams
    PUBLIC FILE_SET CXX_MODULES FILES
        teams/agent_swarms_enabled.cppm
        teams/control_message_compat.cppm
        teams/team_helpers.cppm
        teams/swarm/swarm_backends.cppm
        teams/swarm/swarm_helpers.cppm
        teams/swarm/swarm_pane_observer.cppm
)
target_sources(cc_teams
    PRIVATE
        teams/swarm/swarm_backends_shell.cpp
        teams/swarm/swarm_backends_detect.cpp
        teams/swarm/swarm_backends_tmux.cpp
        teams/swarm/swarm_backends_iterm.cpp
        teams/swarm/swarm_backends_inprocess.cpp
        teams/swarm/swarm_backends_executor.cpp
        teams/swarm/swarm_backends_registry.cpp
        teams/swarm/swarm_backends_detail.cpp
        teams/swarm/swarm_helpers_shard.cpp
)
target_link_libraries(cc_teams
    PUBLIC
        cc_utils
)
