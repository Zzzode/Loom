fn sequence_par_over_control_blocks_render_unicode_frames() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\npar_over Everyone\nA->>B: Work\nend",
        &AsciiRenderOptions::unicode(),
    )
    .expect("par_over should render with Unicode charset");

    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with("┌ par_over Everyone ")),
        "par_over should render a labeled Unicode top frame:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with('│') && line.contains("Work")),
        "par_over should keep contained rows inside the Unicode frame:\n{rendered}"
    );
    assert!(
        rendered.lines().any(|line| line.starts_with('└')),
        "par_over should render a Unicode bottom frame:\n{rendered}"
    );
}