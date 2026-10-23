fn class_parser_composition_relation_renders_unicode_marker() {
    let rendered = render_class(
        "classDiagram\nclass Whole\nclass Part\nWhole *-- Part",
        &AsciiRenderOptions::unicode(),
    )
    .expect("class diagram should render");

    assert_eq!(
        rendered,
        concat!(
            "┌───────┐\n",
            "│ Whole │\n",
            "└───────┘\n",
            "    ◆\n",
            "    │\n",
            "┌──────┐\n",
            "│ Part │\n",
            "└──────┘\n",
        )
    );
}