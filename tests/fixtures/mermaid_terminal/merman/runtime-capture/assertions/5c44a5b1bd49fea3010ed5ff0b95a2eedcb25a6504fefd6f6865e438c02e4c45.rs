fn sequence_control_blocks_support_created_and_destroyed_actors() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nloop Setup\ncreate participant C\nB->>C: Hello C\nC->>B: Still here\ndestroy C\nB--xC: Bye C\nend",
        &AsciiRenderOptions::unicode(),
    )
    .expect("control blocks should support create and destroy lifecycle rows");

    let frame_top = rendered
        .lines()
        .find(|line| line.contains("loop Setup"))
        .unwrap_or_else(|| panic!("loop should render around lifecycle rows:\n{rendered}"));
    let unrelated_lifeline = frame_top
        .find('│')
        .unwrap_or_else(|| panic!("A's lifeline should remain visible:\n{rendered}"));
    let frame_left = frame_top
        .find('┌')
        .unwrap_or_else(|| panic!("the lifecycle frame should have a left border:\n{rendered}"));
    assert!(
        unrelated_lifeline < frame_left,
        "the B-C lifecycle frame should leave unrelated actor A outside:\n{rendered}"
    );
    assert_text_between_borders(&rendered, "Hello C", '│');
    assert_text_between_borders(&rendered, "Bye C", '│');
}