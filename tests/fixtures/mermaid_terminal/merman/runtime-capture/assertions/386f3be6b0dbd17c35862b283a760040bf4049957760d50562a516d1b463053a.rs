fn sequence_actor_lifecycle_renders_from_typed_model() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nA->>B: Before\ncreate participant C\nB->>C: Hello C\nC->>B: Still here\ndestroy C\nB--xC: Bye C\nB->>A: After",
        &AsciiRenderOptions::unicode(),
    )
    .expect("sequence actor create/destroy should render");

    let header = rendered.lines().take(3).collect::<Vec<_>>().join("\n");
    assert!(
        !header.contains("│ C │"),
        "created participant should not render in the initial header:\n{rendered}"
    );
    assert_eq!(
        rendered.matches("│ C │").count(),
        1,
        "created participant should render once at its creation point:\n{rendered}"
    );
    assert!(
        rendered.contains("×"),
        "destroyed participant should render a termination marker:\n{rendered}"
    );
}