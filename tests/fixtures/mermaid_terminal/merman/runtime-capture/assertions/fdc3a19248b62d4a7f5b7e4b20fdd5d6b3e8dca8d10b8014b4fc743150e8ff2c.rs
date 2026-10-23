fn flowchart_parser_simple_subgraph_renders_group_box() {
    let rendered = render_flowchart(
        "flowchart TB\nsubgraph one\nA --> B\nend",
        &AsciiRenderOptions::ascii(),
    )
    .unwrap();

    assert_eq!(
        rendered,
        concat!(
            "+-----------------+\n",
            "|       one       |\n",
            "|                 |\n",
            "|                 |\n",
            "| +---+     +---+ |\n",
            "| |   |     |   | |\n",
            "| | A |---->| B | |\n",
            "| |   |     |   | |\n",
            "| +---+     +---+ |\n",
            "|                 |\n",
            "+-----------------+\n",
        )
    );
}