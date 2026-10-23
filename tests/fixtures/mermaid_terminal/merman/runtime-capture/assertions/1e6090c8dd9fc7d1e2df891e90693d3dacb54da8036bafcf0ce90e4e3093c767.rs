fn flowchart_parser_bt_skip_edge_preserves_dagre_chain_after_vertical_flip() {
    let rendered = render_flowchart(
        "flowchart BT\n  A --> C\n  A --> B\n  B --> C\n",
        &AsciiRenderOptions::ascii(),
    )
    .expect("same-rank bottom-up skip edge should route");

    let line_index = |needle: &str| first_line_index_containing(&rendered, needle);
    assert!(line_index("C") < line_index("B"), "{rendered}");
    assert!(line_index("B") < line_index("A"), "{rendered}");
    assert_eq!(
        rendered
            .chars()
            .filter(|ch| matches!(ch, '>' | '<' | 'v' | '^'))
            .count(),
        3,
        "the two chain edges and skip edge should survive the BT mirror:\n{rendered}"
    );
    assert_rectangular_char_grid(&rendered);
}