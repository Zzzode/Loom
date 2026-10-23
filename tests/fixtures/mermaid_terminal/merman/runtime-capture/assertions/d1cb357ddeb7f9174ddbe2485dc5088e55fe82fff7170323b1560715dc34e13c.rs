fn class_parser_extension_star_renders_all_children() {
    let rendered = render_class(
        "classDiagram\nclass Animal\nclass Dog\nclass Cat\nAnimal <|-- Dog\nAnimal <|-- Cat",
        &AsciiRenderOptions::ascii(),
    )
    .expect("class diagram should render");

    assert_eq!(
        rendered,
        concat!(
            "    +--------+\n",
            "    | Animal |\n",
            "    +--------+\n",
            "         ^\n",
            "         |\n",
            "   +-----+----+\n",
            "+-----+    +-----+\n",
            "| Dog |    | Cat |\n",
            "+-----+    +-----+\n",
        )
    );
}