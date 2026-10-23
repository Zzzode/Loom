fn flowchart_parser_rl_edge_labels_stay_readable() {
    let rendered = render_flowchart(
        "flowchart RL\nA -- hello --> B",
        &AsciiRenderOptions::ascii(),
    )
    .unwrap();

    assert_eq!(
        rendered,
        concat!(
            "+---+          +---+\n",
            "|   |          |   |\n",
            "| B |<--hello--| A |\n",
            "|   |          |   |\n",
            "+---+          +---+\n",
        )
    );
}