fn class_parser_right_left_namespace_to_root_relation_preserves_semantics() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "direction RL\n",
            "namespace Domain {\n  class Service\n}\n",
            "class Gateway\n",
            "Service \"inside\" --> \"outside\" Gateway : exposes",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("right-left namespace-to-root relation should route");

    assert!(!rendered.contains("relations:"), "{rendered}");
    for expected in [
        "Domain", "Service", "Gateway", "inside", "outside", "exposes",
    ] {
        assert!(
            rendered.contains(expected),
            "missing {expected:?}:\n{rendered}"
        );
    }
    let (_, domain_column) = line_and_column_containing(&rendered, "Domain");
    let (_, gateway_column) = line_and_column_containing(&rendered, "Gateway");
    assert!(
        gateway_column < domain_column,
        "RL should place target left of source:\n{rendered}"
    );
}