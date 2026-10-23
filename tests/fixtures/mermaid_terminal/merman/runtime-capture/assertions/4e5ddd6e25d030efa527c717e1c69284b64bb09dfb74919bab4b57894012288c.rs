fn class_parser_horizontal_mixed_self_and_normal_relations_use_lossless_summary() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "direction LR\n",
            "class A\n",
            "class B\n",
            "A --> A : self\n",
            "A --> B : next",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("mixed horizontal class relations should remain recoverable");

    assert_eq!(rendered.matches("| A |").count(), 1, "{rendered}");
    assert_eq!(rendered.matches("| B |").count(), 1, "{rendered}");
    assert!(rendered.contains("relations:"), "{rendered}");
    for expected in [
        framed_class_summary_relation("A", "-->", "A", Some("self")),
        framed_class_summary_relation("A", "-->", "B", Some("next")),
    ] {
        assert!(
            rendered.contains(&expected),
            "missing {expected:?}:\n{rendered}"
        );
    }
}