fn sequence_notes_render_from_typed_model() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nA->>B: Start\nNote right of A: right\nNote left of B: left\nNote over A,B: over both\nB-->>A: Done",
        &AsciiRenderOptions::unicode(),
    )
    .expect("single-line sequence notes should render");

    assert!(
        rendered.contains("│ right │"),
        "right-of note should render as a note box:\n{rendered}"
    );
    assert!(
        rendered.contains("│ left │"),
        "left-of note should render as a note box:\n{rendered}"
    );
    assert!(
        rendered.contains("│ over both │"),
        "over note should render as a note box:\n{rendered}"
    );
    assert!(
        rendered.contains("├────────►│"),
        "normal messages around notes should keep rendering:\n{rendered}"
    );
}