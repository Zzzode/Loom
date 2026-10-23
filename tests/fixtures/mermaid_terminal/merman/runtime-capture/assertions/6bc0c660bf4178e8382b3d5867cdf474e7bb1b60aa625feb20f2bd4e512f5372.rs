fn sequence_wrapped_messages_render_from_typed_model() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nA->>B:wrap: Alpha Beta Gamma",
        &AsciiRenderOptions::unicode(),
    )
    .expect("wrapped sequence messages should render");

    assert!(
        rendered.contains("Alpha") && rendered.contains("Beta") && rendered.contains("Gamma"),
        "wrapped message should keep all words:\n{rendered}"
    );
    assert!(
        !rendered.contains("Alpha Beta Gamma"),
        "wrapped message should not render as one long line:\n{rendered}"
    );
}