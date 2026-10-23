fn sequence_box_keeps_inner_padding_around_participants_and_frames() {
    let rendered = render_sequence(
        "sequenceDiagram\nbox Group\nparticipant A\nparticipant B\nend\nloop Work\nA->>B: Hi\nend",
        &AsciiRenderOptions::ascii(),
    )
    .expect("boxed control block should render");

    assert!(
        rendered
            .lines()
            .any(|line| line.starts_with("| + loop Work ")),
        "control frame should keep one column of padding inside sequence box:\n{rendered}"
    );
    assert!(
        !rendered.lines().any(|line| line.starts_with("|+")),
        "sequence box inner content should not touch the left border:\n{rendered}"
    );
    assert!(
        !rendered.lines().any(|line| line.starts_with("||")),
        "sequence box body rows should not merge with inner frame or participant borders:\n{rendered}"
    );
}