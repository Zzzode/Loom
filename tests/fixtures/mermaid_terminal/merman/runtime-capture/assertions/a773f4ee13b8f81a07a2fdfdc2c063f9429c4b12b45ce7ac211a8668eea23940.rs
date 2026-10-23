fn sequence_global_wrap_does_not_invalidate_empty_activation_records() {
    let rendered = render_sequence(
        "%%{wrap}%%\nsequenceDiagram\nparticipant A\nparticipant B\nactivate B\nA->>B: Work\ndeactivate B",
        &AsciiRenderOptions::unicode(),
    )
    .expect("global wrapping is harmless metadata on empty activation records");

    assert!(rendered.contains("Work") && rendered.contains('┃'));
}