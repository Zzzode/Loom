fn class_parser_mixed_parallel_relationship_layout_renders_each_lane() {
    let rendered = render_class(
        "classDiagram\nclass Animal\nclass Dog\nclass Cat\nAnimal <|-- Dog\nAnimal <|-- Dog\nAnimal <|-- Cat",
        &AsciiRenderOptions::ascii(),
    )
    .expect("mixed parallel class relationships should render every lane");

    assert_eq!(
        rendered,
        concat!(
            "       +--------+\n",
            "       | Animal |\n",
            "       +--------+\n",
            "         ^  ^  ^\n",
            "         |  |  |\n",
            "   +-----+--+--+-+\n",
            "   +-----+    +-----+\n",
            "   | Dog |    | Cat |\n",
            "   +-----+    +-----+\n",
        )
    );
}