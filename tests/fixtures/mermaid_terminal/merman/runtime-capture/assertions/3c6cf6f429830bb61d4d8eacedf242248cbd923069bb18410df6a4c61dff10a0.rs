fn flowchart_parser_top_down_skip_edge_preserves_dagre_chain_ranks() {
    let rendered = render_flowchart(
        concat!(
            "flowchart TD\n",
            "  A --> C\n",
            "  A --> B\n",
            "  B --> C\n",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("same-rank top-down skip edge should route");

    let line_index = |needle: &str| first_line_index_containing(&rendered, needle);
    assert!(line_index("A") < line_index("B"), "{rendered}");
    assert!(line_index("B") < line_index("C"), "{rendered}");
    assert_eq!(
        rendered
            .chars()
            .filter(|ch| matches!(ch, '>' | '<' | 'v' | '^'))
            .count(),
        3,
        "the two chain edges and skip edge should all retain target markers:\n{rendered}"
    );
    assert_rectangular_char_grid(&rendered);
}