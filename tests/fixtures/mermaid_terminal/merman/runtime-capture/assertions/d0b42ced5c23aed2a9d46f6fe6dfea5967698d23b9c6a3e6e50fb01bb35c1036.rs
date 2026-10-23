fn flowchart_parser_dotted_edges_render_with_dotted_line() {
    let rendered =
        render_flowchart("flowchart LR\nA -.-> B", &AsciiRenderOptions::ascii()).unwrap();

    assert_eq!(
        rendered,
        "+---+     +---+\n|   |     |   |\n| A |....>| B |\n|   |     |   |\n+---+     +---+\n"
    );
}