fn flowchart_parser_top_down_skip_edge_label_preserves_arrow() {
    let rendered = render_flowchart(
        "flowchart TD\n  A --> C\n  A --> B\n  B -->|back| C\n",
        &AsciiRenderOptions::unicode(),
    )
    .expect("labeled same-rank top-down edge should route in unicode");

    assert!(rendered.contains("back"), "{rendered}");
    assert_eq!(
        rendered
            .chars()
            .filter(|ch| matches!(ch, '▼' | '◄' | '►' | '▲'))
            .count(),
        3,
        "the labeled chain edge and skip edge should keep all target markers:\n{rendered}"
    );
}