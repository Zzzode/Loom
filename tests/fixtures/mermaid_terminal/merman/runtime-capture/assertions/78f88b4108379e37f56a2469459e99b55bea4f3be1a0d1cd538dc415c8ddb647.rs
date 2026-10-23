fn flowchart_parser_open_edges_render_without_arrowhead() {
    let rendered = render_flowchart("flowchart LR\nA --- B", &AsciiRenderOptions::ascii()).unwrap();

    assert_eq!(
        rendered,
        "+---+     +---+\n|   |     |   |\n| A |-----| B |\n|   |     |   |\n+---+     +---+\n"
    );
}