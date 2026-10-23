fn flowchart_parser_tb_edge_label_renders_between_nodes() {
    let rendered =
        render_flowchart("flowchart TB\nA -- yes --> B", &AsciiRenderOptions::ascii()).unwrap();

    assert_eq!(
        rendered,
        concat!(
            "+-----+\n",
            "|     |\n",
            "|  A  |\n",
            "|     |\n",
            "+-----+\n",
            "   |   \n",
            "   |   \n",
            "  yes  \n",
            "   |   \n",
            "   v   \n",
            "+-----+\n",
            "|     |\n",
            "|  B  |\n",
            "|     |\n",
            "+-----+\n",
        )
    );
}