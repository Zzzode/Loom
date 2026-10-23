fn flowchart_parser_rl_cjk_node_labels_reserve_display_cells() {
    let rendered = render_flowchart(
        "flowchart RL\nA[中A] --> B[终B]",
        &AsciiRenderOptions::ascii(),
    )
    .unwrap();

    assert_eq!(
        rendered,
        concat!(
            "+-----+     +-----+\n",
            "|     |     |     |\n",
            "| 终B |<----| 中A |\n",
            "|     |     |     |\n",
            "+-----+     +-----+\n",
        )
    );
}