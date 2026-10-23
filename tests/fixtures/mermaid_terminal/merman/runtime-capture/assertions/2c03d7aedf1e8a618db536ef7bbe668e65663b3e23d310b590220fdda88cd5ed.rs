fn flowchart_parser_lr_edge_label_renders_on_edge_line() {
    let rendered = render_flowchart(
        "flowchart LR\nA -- hello --> B",
        &AsciiRenderOptions::ascii(),
    )
    .unwrap();

    assert_eq!(
        rendered,
        concat!(
            "+---+          +---+\n",
            "|   |          |   |\n",
            "| A |--hello-->| B |\n",
            "|   |          |   |\n",
            "+---+          +---+\n",
        )
    );
}