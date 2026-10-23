fn sequence_note_left_of_leftmost_actor_reserves_a_lifeline_gutter() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nNote left of A: Left\nA->>B: Ping",
        &AsciiRenderOptions::unicode(),
    )
    .expect("leftmost actor note should render with a dedicated gutter");

    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with('┌') && line.contains("┐  │")),
        "left-of note should end before the leftmost lifeline with the configured gap:\n{rendered}"
    );
    assert!(
        rendered.contains("Left") && rendered.contains("Ping"),
        "note gutters should preserve both note and message content:\n{rendered}"
    );
}