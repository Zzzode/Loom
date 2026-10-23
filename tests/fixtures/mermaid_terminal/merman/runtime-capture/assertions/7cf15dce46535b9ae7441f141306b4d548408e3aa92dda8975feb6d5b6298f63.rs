fn class_parser_dotted_association_relation_renders_plain_dotted_line_without_marker() {
    let rendered = render_class(
        "classDiagram\nclass Student\nclass Course\nStudent .. Course : observes",
        &AsciiRenderOptions::ascii(),
    )
    .expect("class diagram should render");

    assert_eq!(
        rendered,
        concat!(
            "+---------+\n",
            "| Student |\n",
            "+---------+\n",
            "     :\n",
            " observes\n",
            "     :\n",
            "+--------+\n",
            "| Course |\n",
            "+--------+\n",
        )
    );
}