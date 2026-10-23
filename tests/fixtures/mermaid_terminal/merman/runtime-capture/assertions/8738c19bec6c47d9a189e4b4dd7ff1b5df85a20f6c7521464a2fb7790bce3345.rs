fn er_parser_relationship_chain_renders_each_cardinality_and_label() {
    let rendered = render_er(
        "erDiagram\nA ||--|| B : owns\nB ||--|| C : owns",
        &AsciiRenderOptions::ascii(),
    )
    .expect("ER should render");

    assert_eq!(
        rendered,
        concat!(
            "+---+\n", "| A |\n", "+---+\n", " ||\n", "owns\n", "  |\n", " ||\n", "+---+\n",
            "| B |\n", "+---+\n", " ||\n", "owns\n", "  |\n", " ||\n", "+---+\n", "| C |\n",
            "+---+\n",
        )
    );
}