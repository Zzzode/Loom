fn class_parser_endpoint_labels_render_near_relation_endpoints() {
    let rendered = render_class(
        "classDiagram\nclass Customer\nclass Order\nCustomer \"1\" --> \"*\" Order : places",
        &AsciiRenderOptions::ascii(),
    )
    .expect("class diagram should render");

    assert_eq!(
        rendered,
        concat!(
            "+----------+\n",
            "| Customer |\n",
            "+----------+\n",
            "      1\n",
            "      |\n",
            "   places\n",
            "      v\n",
            "      *\n",
            "  +-------+\n",
            "  | Order |\n",
            "  +-------+\n",
        )
    );
}