fn class_parser_dependency_relation_renders_dotted_arrow_marker() {
    let rendered = render_class(
        "classDiagram\nclass Service\nclass Repo\nService ..> Repo : uses",
        &AsciiRenderOptions::ascii(),
    )
    .expect("class diagram should render");

    assert_eq!(
        rendered,
        concat!(
            "+---------+\n",
            "| Service |\n",
            "+---------+\n",
            "     :\n",
            "   uses\n",
            "     v\n",
            " +------+\n",
            " | Repo |\n",
            " +------+\n",
        )
    );
}