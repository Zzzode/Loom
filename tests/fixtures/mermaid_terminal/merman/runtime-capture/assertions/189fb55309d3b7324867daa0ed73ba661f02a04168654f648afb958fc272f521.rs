fn sequence_box_html_breaks_render_multiline_labels() {
    let rendered = render_sequence(
        "sequenceDiagram\nbox First<br/>Second\nparticipant A\nend\nA->>A: Ping",
        &AsciiRenderOptions::ascii(),
    )
    .expect("sequence box labels with HTML breaks should render");
    let normalized = normalize_sequence_output(&rendered);
    let lines = normalized.lines().collect::<Vec<_>>();
    let first = lines
        .iter()
        .position(|line| line.contains("First"))
        .expect("first box label line should render");
    let second = lines
        .iter()
        .position(|line| line.contains("Second"))
        .expect("second box label line should render");

    assert_eq!(
        second,
        first + 1,
        "HTML breaks in box labels should create adjacent label rows:\n{rendered}"
    );
    assert!(
        !rendered.contains("<br"),
        "HTML break markup should not leak into sequence box labels:\n{rendered}"
    );
}