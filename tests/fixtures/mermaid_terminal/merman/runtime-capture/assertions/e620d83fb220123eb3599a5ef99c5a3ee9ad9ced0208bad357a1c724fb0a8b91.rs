fn class_parser_endpoint_labels_are_routed_without_fallback_summary() {
    let rendered = render_class(
        "classDiagram\nclass A\nclass B\nclass C\nA \"1\" --> \"*\" B : ab\nB \"1\" --> \"*\" C : bc",
        &AsciiRenderOptions::ascii(),
    )
    .expect("class diagram should render");

    assert!(
        !rendered.contains("relations:"),
        "endpoint-label fixture should stay routed, not summarize:\n{rendered}"
    );
    assert_eq!(
        rendered,
        concat!(
            "+---+\n", "| A |\n", "+---+\n", "  1\n", "  |\n", " av\n", "  *\n", "+---+\n",
            "| B |\n", "+---+\n", "  1\n", "  |\n", " bv\n", "  *\n", "+---+\n", "| C |\n",
            "+---+\n",
        )
    );
}