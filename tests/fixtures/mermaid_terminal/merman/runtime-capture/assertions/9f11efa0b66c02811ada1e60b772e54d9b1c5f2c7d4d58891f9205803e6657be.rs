fn class_parser_k2_2_relationships_use_a_bounded_planar_layout() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "class A\n",
            "class B\n",
            "class C\n",
            "class D\n",
            "A --> C : ac\n",
            "A o-- D : ad\n",
            "B ..> C : bc\n",
            "B *-- D : bd",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("bounded K2,2 class relationships should render diagrammatically");

    assert!(
        !rendered.contains("relations:"),
        "a strict K2,2 component should use the bounded planar layout:\n{rendered}"
    );
    for expected in ["ac", "ad", "bc", "bd", "^", "v", "o", "*"] {
        assert!(
            rendered.contains(expected),
            "bounded K2,2 output should retain {expected:?}:\n{rendered}"
        );
    }
    for node in ["A", "B", "C", "D"] {
        assert_eq!(
            rendered.matches(&format!("| {node} |")).count(),
            1,
            "bounded K2,2 output should render node {node:?} exactly once:\n{rendered}"
        );
    }
}