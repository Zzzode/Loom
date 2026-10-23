fn sequence_rect_control_blocks_render_unicode_frames() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nrect rgba(0,0,0,0.1)\nA->>B: Shaded\nend",
        &AsciiRenderOptions::unicode(),
    )
    .expect("rect should render with Unicode charset");

    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with("┌ rect rgba(0,0,0,0.1) ")),
        "rect should render a labeled Unicode top frame:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with('│') && line.contains("Shaded")),
        "rect should keep contained rows inside the Unicode frame:\n{rendered}"
    );
    assert!(
        rendered.lines().any(|line| line.starts_with('└')),
        "rect should render a Unicode bottom frame:\n{rendered}"
    );
}