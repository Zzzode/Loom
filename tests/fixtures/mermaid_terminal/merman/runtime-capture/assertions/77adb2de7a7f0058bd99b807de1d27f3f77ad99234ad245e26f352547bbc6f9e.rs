fn sequence_sectioned_control_blocks_render_ascii_frames() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nalt Success\nA->>B: OK\nelse Failure\nB-->>A: Retry\nend",
        &AsciiRenderOptions::ascii(),
    )
    .expect("alt should render with ASCII charset");

    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with("+ alt Success ")),
        "alt should render a labeled ASCII top frame:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with("+ else Failure ")),
        "alt should render a labeled ASCII section separator:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with('|') && line.contains("Retry")),
        "alt should keep second section rows inside the ASCII frame:\n{rendered}"
    );
}