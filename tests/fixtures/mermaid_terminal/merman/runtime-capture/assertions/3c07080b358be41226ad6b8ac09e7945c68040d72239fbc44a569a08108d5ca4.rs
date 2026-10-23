fn class_parser_reverse_extension_orients_marker_toward_parent() {
    let rendered = render_class(
        "classDiagram\nclass Animal\nclass Dog\nDog --|> Animal",
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
            "  +-----+\n",
            "  | Dog |\n",
            "  +-----+\n",
        )
    );
}