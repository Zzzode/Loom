fn er_parser_relationship_layouts_render_unrelated_entities_as_components() {
    let rendered = render_er(
        "erDiagram\nA ||--|| B : owns\nC",
        &AsciiRenderOptions::ascii(),
    )
    .expect("unrelated ER entities should render as separate components");

    assert_eq!(
        rendered,
        concat!(
            "+---+\n", "| A |\n", "+---+\n", " ||\n", "owns\n", "  |\n", " ||\n", "+---+\n",
            "| B |\n", "+---+\n", "\n", "+---+\n", "| C |\n", "+---+\n",
        )
    );
}