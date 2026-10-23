fn class_parser_aggregation_relation_renders_hollow_diamond_marker() {
    let rendered = render_class(
        "classDiagram\nclass Whole\nclass Part\nWhole o-- Part : owns",
        &AsciiRenderOptions::ascii(),
    )
    .expect("class diagram should render");

    assert_eq!(
        rendered,
        concat!(
            "+-------+\n",
            "| Whole |\n",
            "+-------+\n",
            "    o\n",
            "  owns\n",
            "    |\n",
            "+------+\n",
            "| Part |\n",
            "+------+\n",
        )
    );
}