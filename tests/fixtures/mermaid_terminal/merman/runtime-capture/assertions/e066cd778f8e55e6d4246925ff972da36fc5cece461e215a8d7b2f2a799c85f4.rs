fn sequence_wrapped_notes_render_from_typed_model() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nNote over A,B:wrap: Alpha Beta Gamma Delta Epsilon Zeta",
        &AsciiRenderOptions::unicode(),
    )
    .expect("wrapped sequence notes should render");

    assert!(
        rendered.contains("Alpha") && rendered.contains("Zeta"),
        "wrapped note should keep all words:\n{rendered}"
    );
    assert!(
        !rendered.contains("Alpha Beta Gamma Delta Epsilon Zeta"),
        "wrapped note should not render as one long line:\n{rendered}"
    );
}