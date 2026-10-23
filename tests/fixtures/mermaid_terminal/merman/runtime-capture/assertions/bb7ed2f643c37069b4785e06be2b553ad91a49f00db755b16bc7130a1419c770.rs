fn er_parser_relationship_star_renders_each_label_and_leaf_cardinality() {
    let rendered = render_er(
        "erDiagram\nA ||--|| B : owns\nA ||--|| C : owns",
        &AsciiRenderOptions::ascii(),
    )
    .expect("ER should render");

    assert_eq!(
        rendered,
        concat!(
            "     +---+\n",
            "     | A |\n",
            "     +---+\n",
            "      ||\n",
            "  owns owns\n",
            "  +----+---+\n",
            " ||       ||\n",
            "+---+    +---+\n",
            "| B |    | C |\n",
            "+---+    +---+\n",
        )
    );
}