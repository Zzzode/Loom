fn flowchart_parser_cjk_and_emoji_labels_reserve_terminal_cells() {
    let rendered = render_flowchart(
        "flowchart LR\nA[开始] -->|处理🚀| B[完成]",
        &AsciiRenderOptions::ascii(),
    )
    .unwrap();

    for expected in ["开始", "处理🚀", "完成"] {
        assert!(
            rendered.contains(expected),
            "wide label {expected:?} should stay visible:\n{rendered}"
        );
    }
    assert_rectangular_terminal_grid(&rendered);
}