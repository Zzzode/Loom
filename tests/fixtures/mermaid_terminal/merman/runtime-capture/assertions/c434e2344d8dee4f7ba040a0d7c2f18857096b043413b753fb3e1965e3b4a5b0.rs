fn sequence_control_blocks_support_activation_lifecycle_rows() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nloop Work\nA->>+B: Start\nB-->>-A: Done\nend",
        &AsciiRenderOptions::unicode(),
    )
    .expect("control blocks should support activation rows");

    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with("┌ loop Work ")),
        "loop should render while activation events are present:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with('│') && line.contains('┃')),
        "active lifeline should remain visible inside the frame:\n{rendered}"
    );
}