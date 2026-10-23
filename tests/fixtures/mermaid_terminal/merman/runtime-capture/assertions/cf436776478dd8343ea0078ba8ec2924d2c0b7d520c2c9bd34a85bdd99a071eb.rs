fn class_parser_relationship_layouts_render_unrelated_classes_as_components() {
    let rendered = render_class(
        "classDiagram\nclass Animal\nclass Dog\nclass Cat\nAnimal <|-- Dog",
        &AsciiRenderOptions::ascii(),
    )
    .expect("unrelated classes should render as separate components");

    assert_eq!(
        rendered,
        concat!(
            "+--------+\n",
            "| Animal |\n",
            "+--------+\n",
            "     ^\n",
            "     |\n",
            "  +-----+\n",
            "  | Dog |\n",
            "  +-----+\n",
            "\n",
            "+-----+\n",
            "| Cat |\n",
            "+-----+\n",
        )
    );
}