fn class_parser_association_relation_renders_plain_line_without_marker() {
    let rendered = render_class(
        "classDiagram\nclass Student\nclass Course\nStudent -- Course : enrolls",
        &AsciiRenderOptions::ascii(),
    )
    .expect("class diagram should render");

    assert_eq!(
        rendered,
        concat!(
            "+---------+\n",
            "| Student |\n",
            "+---------+\n",
            "     |\n",
            "  enrolls\n",
            "     |\n",
            "+--------+\n",
            "| Course |\n",
            "+--------+\n",
        )
    );
}