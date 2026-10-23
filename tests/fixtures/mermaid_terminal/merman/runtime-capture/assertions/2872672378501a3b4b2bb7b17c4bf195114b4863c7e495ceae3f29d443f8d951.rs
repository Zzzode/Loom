fn er_parser_reversed_one_or_more_cardinality_renders_normalized_marker() {
    let rendered = render_er("erDiagram\nA }|--|| B : has", &AsciiRenderOptions::ascii())
        .expect("ER should render");

    assert_eq!(
        rendered,
        concat!(
            "+---+\n", "| A |\n", "+---+\n", " |{\n", " has\n", "  |\n", " ||\n", "+---+\n",
            "| B |\n", "+---+\n",
        )
    );
}