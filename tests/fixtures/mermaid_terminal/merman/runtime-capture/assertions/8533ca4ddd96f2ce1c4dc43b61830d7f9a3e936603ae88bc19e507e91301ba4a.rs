fn sequence_note_html_breaks_render_multiline_note_boxes() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nA->>A: Ping\nNote right of A: First<br/>Second",
        &AsciiRenderOptions::ascii(),
    )
    .expect("sequence should render");
    let lines = normalize_sequence_output(&rendered)
        .lines()
        .map(ToOwned::to_owned)
        .collect::<Vec<_>>();
    let first = lines
        .iter()
        .position(|line| line.contains("First"))
        .expect("first note line should render");
    let second = lines
        .iter()
        .position(|line| line.contains("Second"))
        .expect("second note line should render");

    assert_eq!(
        second,
        first + 1,
        "HTML line breaks should become adjacent note content rows:\n{rendered}"
    );
    assert!(
        !rendered.contains("<br"),
        "HTML break markup should not leak into ASCII output:\n{rendered}"
    );
}