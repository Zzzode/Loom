fn state_group_transition_endpoints_attach_to_group_boundary() {
    let rendered = render_state(
        "stateDiagram-v2\nstate Parent {\n  Child\n}\nA --> Parent",
        &AsciiRenderOptions::ascii(),
    )
    .expect("state transitions should be able to target composite state boundaries");

    assert!(
        rendered.contains("Parent"),
        "target composite state should render as a group label:\n{rendered}"
    );
    assert!(
        rendered.contains("Child"),
        "target composite state should keep its child state visible:\n{rendered}"
    );
    assert!(
        rendered.contains("A"),
        "source state should render outside the target group:\n{rendered}"
    );
}