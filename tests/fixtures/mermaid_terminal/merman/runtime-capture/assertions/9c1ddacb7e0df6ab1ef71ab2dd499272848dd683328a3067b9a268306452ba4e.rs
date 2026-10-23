fn class_parser_cross_namespace_collision_keeps_lossless_summary() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "namespace Left {\n  class A\n  class B\n}\n",
            "namespace Right {\n  class C\n  class D\n}\n",
            "A --> C : ac\n",
            "A --> D : ad\n",
            "B --> C : bc\n",
            "B --> D : bd",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("cross-namespace collision should retain a lossless fallback");

    assert!(rendered.contains("relations:"), "{rendered}");
    for expected in ["A", "B", "C", "D", "ac", "ad", "bc", "bd"] {
        assert!(
            rendered.contains(expected),
            "missing {expected:?}:\n{rendered}"
        );
    }
}