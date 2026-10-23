fn state_start_and_end_pseudo_states_render_as_distinct_visible_nodes() {
    let rendered = render_state(
        "stateDiagram-v2\n[*] --> A\nA --> [*]",
        &AsciiRenderOptions::ascii(),
    )
    .expect("start and end pseudo states should render");

    assert!(
        rendered.contains("| * |"),
        "start pseudo state should render as a visible star node:\n{rendered}"
    );
    assert!(
        rendered.contains("| @ |"),
        "end pseudo state should render with a distinct terminal symbol:\n{rendered}"
    );
    assert!(
        !rendered.contains("root_start") && !rendered.contains("root_end"),
        "start/end implementation ids should not leak into ASCII output:\n{rendered}"
    );
}