fn sequence_nested_control_blocks_render() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nloop Outer\nopt Inner\nA->>B: Work\nend\nend",
        &AsciiRenderOptions::unicode(),
    )
    .expect("nested control blocks should render");

    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with("┌ loop Outer ")),
        "outer frame should render:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with("│ ┌ opt Inner ")),
        "inner frame should render inside the outer frame:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with("│ │") && line.contains("Work")),
        "message rows should stay inside both frames:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with("│ ├") && line.contains('►')),
        "nested frame borders must not replace the message source junction:\n{rendered}"
    );
}