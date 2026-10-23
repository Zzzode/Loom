fn er_parser_reversed_zero_or_more_cardinality_renders_normalized_marker() {
    let rendered = render_er("erDiagram\nA }o--|| B : has", &AsciiRenderOptions::ascii())
        .expect("ER should render");

    assert_eq!(
        rendered,
        concat!(
            "+---+\n", "| A |\n", "+---+\n", " o{\n", " has\n", "  |\n", " ||\n", "+---+\n",
            "| B |\n", "+---+\n",
        )
    );
}