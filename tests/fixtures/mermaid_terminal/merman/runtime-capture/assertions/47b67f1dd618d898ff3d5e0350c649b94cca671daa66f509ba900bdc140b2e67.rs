fn flowchart_node_label_wrapping_preserves_authored_breaks_and_graphemes() {
    let input = "flowchart TD\nA[\"é team 👩‍💻<br>超長標籤 abcdefghijklmnop\"] --> B[Done]";
    let options = AsciiRenderOptions::unicode().with_flowchart_node_label_wrap_width(8);
    let rendered = render_flowchart(input, &options).expect("wrapped Unicode label should render");

    for expected in [
        "é",
        "team",
        "👩‍💻",
        "超長標籤",
        "abcdefgh",
        "ijklmnop",
        "Done",
    ] {
        assert!(
            rendered.contains(expected),
            "missing complete grapheme/text fragment {expected:?}:\n{rendered}"
        );
    }
    let emoji_row = first_line_index_containing(&rendered, "👩‍💻");
    let cjk_row = first_line_index_containing(&rendered, "超長");
    assert!(
        cjk_row > emoji_row,
        "the authored <br> must remain a hard break:\n{rendered}"
    );
    assert_rectangular_terminal_grid(&rendered);
}