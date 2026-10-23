fn sequence_single_section_control_blocks_render_ascii_frames() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nloop Every minute\nA->>B: Ping\nend",
        &AsciiRenderOptions::ascii(),
    )
    .expect("loop should render with ASCII charset");

    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with("+ loop Every minute ")),
        "loop should render a labeled ASCII top frame:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with('|') && line.contains("Ping")),
        "loop should keep contained rows inside the ASCII frame:\n{rendered}"
    );
    assert!(
        rendered.lines().any(|line| line.starts_with('+')),
        "loop should render an ASCII bottom frame:\n{rendered}"
    );
}