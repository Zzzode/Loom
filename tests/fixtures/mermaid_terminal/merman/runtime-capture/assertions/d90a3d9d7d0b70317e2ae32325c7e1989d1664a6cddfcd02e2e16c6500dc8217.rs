fn class_parser_independent_relationships_render_as_compact_components() {
    let rendered = render_class(
        "classDiagram\nclass A\nclass B\nclass C\nclass D\nA <|-- D\nB <|-- C",
        &AsciiRenderOptions::ascii(),
    )
    .expect("independent class relationships should render as separate components");

    assert_eq!(
        rendered,
        concat!(
            "+---+    +---+\n",
            "| A |    | B |\n",
            "+---+    +---+\n",
            "  ^        ^\n",
            "  |        |\n",
            "+---+    +---+\n",
            "| D |    | C |\n",
            "+---+    +---+\n",
        )
    );
}