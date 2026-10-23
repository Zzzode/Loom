fn class_parser_dense_plain_associations_keep_summary_connector() {
    let rendered = render_class(
        "classDiagram\nclass A\nclass B\nclass C\nA -- B : ab\nB -- A : ba\nA -- C : ac\nC -- A : ca\nB -- C : bc\nC -- B : cb",
        &AsciiRenderOptions::ascii(),
    )
    .expect("dense plain associations should render through relation summary fallback");

    assert!(
        rendered.contains("relations:"),
        "dense plain association fixture should use relation summary:\n{rendered}"
    );
    for expected in [
        framed_class_summary_relation("A", "--", "B", Some("ab")),
        framed_class_summary_relation("B", "--", "A", Some("ba")),
        framed_class_summary_relation("A", "--", "C", Some("ac")),
        framed_class_summary_relation("C", "--", "A", Some("ca")),
        framed_class_summary_relation("B", "--", "C", Some("bc")),
        framed_class_summary_relation("C", "--", "B", Some("cb")),
    ] {
        assert!(
            rendered.contains(&expected),
            "dense plain association summary should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        !rendered.contains("-->") && !rendered.contains("<--"),
        "dense plain association summary should not invent arrowheads:\n{rendered}"
    );
}