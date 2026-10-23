fn sequence_par_over_control_blocks_render_ascii_frames() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\npar_over Everyone\nA->>B: Work\nend",
        &AsciiRenderOptions::ascii(),
    )
    .expect("par_over should render with ASCII charset");

    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with("+ par_over Everyone ")),
        "par_over should render a labeled ASCII top frame:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with('|') && line.contains("Work")),
        "par_over should keep contained rows inside the ASCII frame:\n{rendered}"
    );
    assert!(
        rendered.lines().any(|line| line.starts_with('+')),
        "par_over should render an ASCII bottom frame:\n{rendered}"
    );
}