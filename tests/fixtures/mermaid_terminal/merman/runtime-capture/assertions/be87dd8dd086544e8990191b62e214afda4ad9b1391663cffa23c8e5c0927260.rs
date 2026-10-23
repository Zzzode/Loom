fn class_parser_horizontal_shared_source_crossings_use_lossless_summary() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "direction LR\n",
            "class A\n",
            "class B\n",
            "class C\n",
            "A --> B : short\n",
            "A --> C : long",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("shared-source horizontal crossings should remain recoverable");

    assert!(rendered.contains("relations:"), "{rendered}");
    for expected in [
        framed_class_summary_relation("A", "-->", "B", Some("short")),
        framed_class_summary_relation("A", "-->", "C", Some("long")),
    ] {
        assert!(
            rendered.contains(&expected),
            "summary must preserve {expected:?} after shared-source crossing fallback:\n{rendered}"
        );
    }
}