fn sequence_single_section_control_blocks_frame_notes() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nloop Watch\nNote over A,B: Wait\nA->>B: Continue\nend",
        &AsciiRenderOptions::unicode(),
    )
    .expect("loop should frame notes and messages");

    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with('│') && line.contains("Wait")),
        "loop should keep note rows inside the frame:\n{rendered}"
    );
    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with('│') && line.contains("Continue")),
        "loop should keep later message rows inside the same frame:\n{rendered}"
    );
}