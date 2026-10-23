fn flowchart_parser_top_down_skip_edge_keeps_unicode_connectivity() {
    let rendered = render_flowchart(
        "flowchart TD\n  A --> C\n  A --> B\n  B --> C\n",
        &AsciiRenderOptions::unicode(),
    )
    .expect("same-rank top-down skip edge should route in unicode");

    let line_index = |needle: &str| first_line_index_containing(&rendered, needle);
    assert!(line_index("A") < line_index("B"), "{rendered}");
    assert!(line_index("B") < line_index("C"), "{rendered}");
    assert_eq!(
        rendered
            .chars()
            .filter(|ch| matches!(ch, '▼' | '◄' | '►' | '▲'))
            .count(),
        3,
        "unicode skip and chain edges should keep all target markers:\n{rendered}"
    );
}