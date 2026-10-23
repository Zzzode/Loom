fn flowchart_parser_thick_top_down_edges_render_with_heavy_ascii_line() {
    let rendered = render_flowchart("flowchart TB\nA ==> B", &AsciiRenderOptions::ascii()).unwrap();

    assert_eq!(
        rendered,
        concat!(
            "+---+\n", "|   |\n", "| A |\n", "|   |\n", "+---+\n", "  #  \n", "  #  \n", "  #  \n",
            "  #  \n", "  v  \n", "+---+\n", "|   |\n", "| B |\n", "|   |\n", "+---+\n",
        )
    );
}