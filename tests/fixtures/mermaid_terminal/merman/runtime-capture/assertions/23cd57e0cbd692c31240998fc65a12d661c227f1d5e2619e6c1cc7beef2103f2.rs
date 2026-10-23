fn sequence_message_labels_reserve_display_cells_for_cjk() {
    let rendered = render_sequence(
        "sequenceDiagram\nparticipant A\nparticipant B\nA->>B: 数据",
        &AsciiRenderOptions::ascii(),
    )
    .expect("CJK sequence messages should render");

    let label_line = rendered
        .lines()
        .find(|line| line.contains("数据"))
        .expect("message label should be rendered");
    let arrow_line = rendered
        .lines()
        .find(|line| line.contains('>'))
        .expect("message arrow should be rendered");

    assert_eq!(
        cjk_test_width(label_line),
        cjk_test_width(arrow_line),
        "message labels should reserve the same terminal columns as the arrow row:\n{rendered}"
    );
}