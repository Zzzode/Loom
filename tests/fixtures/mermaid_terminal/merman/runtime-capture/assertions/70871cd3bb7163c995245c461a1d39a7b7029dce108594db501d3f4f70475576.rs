fn state_fork_and_join_pseudo_states_render_as_sync_bars() {
    let rendered = render_state(
        "stateDiagram-v2\nstate fork_state <<fork>>\n[*] --> fork_state\nfork_state --> State2\nfork_state --> State3\nstate join_state <<join>>\nState2 --> join_state\nState3 --> join_state\njoin_state --> State4\nState4 --> [*]",
        &AsciiRenderOptions::ascii(),
    )
    .expect("fork and join pseudo states should render");

    assert!(
        rendered.lines().any(|line| line.contains("State2"))
            && rendered.lines().any(|line| line.contains("State3"))
            && rendered.lines().any(|line| line.contains("State4")),
        "fork/join branches should keep their target states visible:\n{rendered}"
    );
    assert!(
        rendered.contains("======="),
        "fork/join pseudo states should render as thick synchronization bars:\n{rendered}"
    );
    assert!(
        !rendered.contains("fork_state") && !rendered.contains("join_state"),
        "fork/join implementation ids should not leak into ASCII output:\n{rendered}"
    );
}