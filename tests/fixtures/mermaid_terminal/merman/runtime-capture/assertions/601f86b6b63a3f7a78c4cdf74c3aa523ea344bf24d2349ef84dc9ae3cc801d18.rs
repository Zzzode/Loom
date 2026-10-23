fn sequence_actor_label_html_breaks_render_multiline_participant_boxes() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A as First<br />Line\nA->>A: Hi",
        &AsciiRenderOptions::ascii(),
    )
    .expect("sequence should render");
    let normalized = normalize_sequence_output(&rendered);
    let lines = normalized.lines().collect::<Vec<_>>();

    assert!(
        lines.len() >= 5,
        "rendered sequence should include multiline participant header:\n{rendered}"
    );
    assert_eq!(lines[0], "+-------+");
    assert_eq!(lines[1], "| First |");
    assert_eq!(lines[2], "| Line  |");
    assert_eq!(lines[3], "+---+---+");
    assert_eq!(lines[4], "    |");
}