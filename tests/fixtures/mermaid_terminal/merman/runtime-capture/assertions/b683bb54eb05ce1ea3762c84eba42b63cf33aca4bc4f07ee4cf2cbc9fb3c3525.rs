fn class_parser_dotted_namespace_keeps_implicit_ancestor_ownership() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "namespace Company.Project.Module {\n",
            "  class A\n",
            "}",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("dotted namespace hierarchy should render");

    let namespace_positions = ["Company", "Project", "Module"].map(|namespace| {
        assert_eq!(
            rendered.matches(namespace).count(),
            1,
            "namespace label {namespace:?} should have one owning container:\n{rendered}"
        );
        line_and_column_containing(&rendered, namespace)
    });
    assert!(
        namespace_positions
            .windows(2)
            .all(|pair| pair[0].0 < pair[1].0 && pair[0].1 < pair[1].1),
        "dotted namespace ancestors should form three nested containers:\n{rendered}"
    );
    assert_eq!(
        rendered.matches("| A |").count(),
        1,
        "the class should remain inside the complete namespace hierarchy:\n{rendered}"
    );
    let class_position = line_and_column_containing(&rendered, " A ");
    assert!(
        namespace_positions[2].0 < class_position.0 && namespace_positions[2].1 < class_position.1,
        "the class should remain nested inside Module:\n{rendered}"
    );
}