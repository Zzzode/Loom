fn flowchart_parser_long_subgraph_title_wraps_to_multiple_rows() {
    let rendered = render_flowchart(
        "flowchart LR\nsubgraph cluster [Wrap this title nicely]\ndirection LR\nA --> B\nend",
        &AsciiRenderOptions::ascii(),
    )
    .expect("long subgraph titles should wrap inside the existing group box");

    assert_eq!(
        rendered,
        concat!(
            "+-----------------+\n",
            "| Wrap this title |\n",
            "|                 |\n",
            "|     nicely      |\n",
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