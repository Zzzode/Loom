fn class_parser_bottom_up_namespace_external_relation_orders_the_target_first() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "direction BT\n",
            "namespace Domain {\n",
            "  class A\n",
            "}\n",
            "class B\n",
            "A --> B : leaves",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("bottom-up external namespace relation should render");

    assert!(
        first_line_index_containing(&rendered, "| B |")
            < first_line_index_containing(&rendered, "| A |"),
        "BT should place the external target before the namespace source:\n{rendered}"
    );
    assert!(!rendered.contains("relations:"), "{rendered}");
    assert!(
        rendered.contains("A") && rendered.contains("leaves"),
        "{rendered}"
    );
}