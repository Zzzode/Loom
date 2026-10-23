fn sequence_control_blocks_render_inside_participant_boxes() {
    let rendered = render_sequence(
        "sequenceDiagram\nbox Group\nparticipant A\nparticipant B\nend\nloop Work\nA->>B: Hi\nend",
        &AsciiRenderOptions::unicode(),
    )
    .expect("control blocks should render with boxed participants");

    assert!(
        rendered.contains("Group"),
        "participant box label should still render:\n{rendered}"
    );
    assert!(
        rendered.contains("loop Work"),
        "control frame should still render inside participant box output:\n{rendered}"
    );
}