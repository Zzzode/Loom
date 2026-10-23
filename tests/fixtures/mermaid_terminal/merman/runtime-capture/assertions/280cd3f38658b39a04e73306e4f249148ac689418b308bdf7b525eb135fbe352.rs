fn sequence_control_frame_uses_the_local_participant_span() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nparticipant C\nA->>C: kickoff\nloop poll\nB->>C: check\nend",
        &AsciiRenderOptions::ascii(),
    )
    .expect("a partial-span control frame should render");

    let frame_top = rendered
        .lines()
        .find(|line| line.contains("loop poll"))
        .unwrap_or_else(|| panic!("loop frame should render:\n{rendered}"));
    let outer_lifeline = frame_top.find('|').unwrap_or_else(|| {
        panic!("A's lifeline should remain visible outside the frame:\n{rendered}")
    });
    let frame_left = frame_top
        .find('+')
        .unwrap_or_else(|| panic!("the local frame should have a left border:\n{rendered}"));
    assert!(
        outer_lifeline < frame_left,
        "the loop frame should start after A's unrelated lifeline:\n{rendered}"
    );

    let message_row = rendered
        .lines()
        .find(|line| line.contains("check"))
        .unwrap_or_else(|| panic!("the local message should remain visible:\n{rendered}"));
    let frame_border = message_row
        .match_indices('|')
        .nth(1)
        .map(|(index, _)| index)
        .unwrap_or_else(|| {
            panic!("the local frame body should preserve both borders:\n{rendered}")
        });
    assert!(
        message_row[..frame_border].contains('|'),
        "A's lifeline should remain outside the local frame body:\n{rendered}"
    );
}