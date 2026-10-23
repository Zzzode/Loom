fn class_parser_extension_chain_renders_each_relationship() {
    let rendered = render_class(
        "classDiagram\nclass Animal\nclass Mammal\nclass Dog\nAnimal <|-- Mammal\nMammal <|-- Dog",
        &AsciiRenderOptions::ascii(),
    )
    .expect("class diagram should render");

    assert_eq!(
        rendered,
        concat!(
            "+--------+\n",
            "| Animal |\n",
            "+--------+\n",
            "     ^\n",
            "     |\n",
            "     |\n",
            "+--------+\n",
            "| Mammal |\n",
            "+--------+\n",
            "     ^\n",
            "     |\n",
            "     |\n",
            "  +-----+\n",
            "  | Dog |\n",
            "  +-----+\n",
        )
    );
}