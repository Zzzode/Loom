fn flowchart_parser_bt_multiline_edge_label_preserves_authored_line_order() {
    let rendered = render_flowchart(
        "flowchart BT\nA -->|first<br>second| B",
        &AsciiRenderOptions::ascii(),
    )
    .expect("BT multiline edge labels should render without reversing their text");

    let first = first_line_index_containing(&rendered, "first");
    let second = first_line_index_containing(&rendered, "second");
    assert_eq!(
        second,
        first + 1,
        "vertical mirroring should preserve authored label row order:\n{rendered}"
    );
}