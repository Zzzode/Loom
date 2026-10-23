fn flowchart_parser_multiline_subgraph_title_renders_centered_rows() {
    let rendered = render_flowchart(
        "flowchart TB\nsubgraph cluster [Line<br>Two]\nA\nend",
        &AsciiRenderOptions::ascii(),
    )
    .expect("subgraph titles with Mermaid break syntax should render as multiline title rows");

    assert_eq!(
        rendered,
        concat!(
            "+-------+\n",
            "| Line  |\n",
            "|       |\n",
            "|  Two  |\n",
            "|       |\n",
            "|       |\n",
            "| +---+ |\n",
            "| |   | |\n",
            "| | A | |\n",
            "| |   | |\n",
            "| +---+ |\n",
            "|       |\n",
            "+-------+\n",
        )
    );
}