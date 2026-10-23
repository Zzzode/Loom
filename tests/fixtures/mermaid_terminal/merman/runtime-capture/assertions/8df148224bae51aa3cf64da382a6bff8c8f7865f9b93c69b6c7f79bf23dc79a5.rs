fn class_parser_horizontal_unrelated_edge_crossings_use_lossless_summary() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "direction LR\n",
            "class A\n",
            "class B\n",
            "class C\n",
            "class D\n",
            "A --> C : first\n",
            "B ..> D : second\n",
            "A --> B : bridge",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("unrelated horizontal crossings should remain recoverable");

    assert!(rendered.contains("relations:"), "{rendered}");
    for expected in [
        framed_class_summary_relation("A", "-->", "C", Some("first")),
        framed_class_summary_relation("B", "..>", "D", Some("second")),
        framed_class_summary_relation("A", "-->", "B", Some("bridge")),
    ] {
        assert!(
            rendered.contains(&expected),
            "summary must preserve {expected:?} after owner crossing fallback:\n{rendered}"
        );
    }
}