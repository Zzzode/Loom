fn class_parser_lollipop_relation_renders_interface_node() {
    let rendered = render_class(
        "classDiagram\nIService ()-- Service",
        &AsciiRenderOptions::ascii(),
    )
    .expect("class diagram should render");

    assert_eq!(
        rendered,
        concat!(
            "+---------------+\n",
            "| <<interface>> |\n",
            "| IService      |\n",
            "+---------------+\n",
            "        o\n",
            "        |\n",
            "   +---------+\n",
            "   | Service |\n",
            "   +---------+\n",
        )
    );
}