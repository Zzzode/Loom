fn class_parser_reverse_extension_endpoint_labels_follow_normalized_endpoints() {
    let rendered = render_class(
        "classDiagram\nclass Child\nclass Parent\nChild \"*\" --|> \"1\" Parent : extends",
        &AsciiRenderOptions::ascii(),
    )
    .expect("class diagram should render");

    assert_eq!(
        rendered,
        concat!(
            "+--------+\n",
            "| Parent |\n",
            "+--------+\n",
            "     1\n",
            "     ^\n",
            "  extends\n",
            "     |\n",
            "     *\n",
            " +-------+\n",
            " | Child |\n",
            " +-------+\n",
        )
    );
}