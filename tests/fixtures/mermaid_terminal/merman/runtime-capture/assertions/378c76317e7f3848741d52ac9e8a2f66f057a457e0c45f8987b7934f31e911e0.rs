fn state_composite_entry_transition_attaches_to_group_boundary() {
    let rendered = render_state(
        "stateDiagram-v2\n[*] --> Active\nstate Active {\n  [*] --> A\n  A --> B\n}",
        &AsciiRenderOptions::ascii(),
    )
    .expect("entry transitions should attach to composite state boundaries");

    assert!(
        rendered.contains("Active"),
        "composite state title should render:\n{rendered}"
    );
    assert!(
        rendered.contains("A") && rendered.contains("B"),
        "composite state children should render:\n{rendered}"
    );
    assert!(
        rendered.matches("| * |").count() >= 2,
        "root and nested start pseudo states should render:\n{rendered}"
    );
}