fn class_parser_class_and_namespace_same_id_keep_distinct_route_ownership() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "class Domain\n",
            "namespace Domain {\n",
            "  class A\n",
            "}\n",
            "Domain --> A : owns",
        ),
        &AsciiRenderOptions::unicode(),
    )
    .expect("class and namespace semantic ids should occupy separate render-id domains");

    assert!(!rendered.contains("relations:"), "{rendered}");
    assert_eq!(
        rendered.matches("│ Domain │").count(),
        2,
        "the class and namespace titles should each render once:\n{rendered}"
    );
    assert_eq!(rendered.matches("│ A │").count(), 1, "{rendered}");
    assert!(rendered.contains("owns"), "{rendered}");
}