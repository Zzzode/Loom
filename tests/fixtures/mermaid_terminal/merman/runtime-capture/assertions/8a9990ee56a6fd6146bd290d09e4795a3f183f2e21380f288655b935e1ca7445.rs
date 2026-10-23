fn sequence_activations_render_from_typed_model() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nA->>+B: Start\nB-->>A: Working\nB-->>-A: Done",
        &AsciiRenderOptions::unicode(),
    )
    .expect("sequence activations should render");

    assert!(
        rendered.contains("┃"),
        "active participant lifeline should render with an activation bar:\n{rendered}"
    );
    assert!(
        rendered.contains("│ Working"),
        "messages should still render while a participant is active:\n{rendered}"
    );
}