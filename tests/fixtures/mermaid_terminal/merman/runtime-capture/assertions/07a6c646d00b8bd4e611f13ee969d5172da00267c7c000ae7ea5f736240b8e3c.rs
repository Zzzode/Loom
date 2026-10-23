fn flowchart_parser_bt_root_direction_renders_with_vertical_flip() {
    let rendered = render_flowchart("flowchart BT\nA --> B", &AsciiRenderOptions::ascii())
        .expect("BT flowchart direction should render as a vertical flip of TD");

    assert_eq!(
        rendered,
        concat!(
            "+---+\n", "|   |\n", "| B |\n", "|   |\n", "+---+\n", "  ^  \n", "  |  \n", "  |  \n",
            "  |  \n", "  |  \n", "+---+\n", "|   |\n", "| A |\n", "|   |\n", "+---+\n",
        )
    );
}