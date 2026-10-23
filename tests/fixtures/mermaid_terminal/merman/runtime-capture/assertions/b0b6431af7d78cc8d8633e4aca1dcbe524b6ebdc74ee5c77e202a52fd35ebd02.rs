fn class_parser_dense_realization_relationships_keep_dotted_summary_connector() {
    let rendered = render_class(
        "classDiagram\nclass A\nclass B\nclass C\nA ..|> B : ab\nB ..|> A : ba\nA ..> C : ac\nC ..> A : ca\nB --> C : bc\nC --> B : cb",
        &AsciiRenderOptions::ascii(),
    )
    .expect("dense realization relationships should render through relation summary fallback");

    assert!(
        rendered.contains("relations:"),
        "dense realization fixture should use relation summary:\n{rendered}"
    );
    for expected in [
        framed_class_summary_relation("B", "<|..", "A", Some("ab")),
        framed_class_summary_relation("A", "<|..", "B", Some("ba")),
        framed_class_summary_relation("A", "..> ", "C", Some("ac")),
        framed_class_summary_relation("B", "--> ", "C", Some("bc")),
    ] {
        assert!(
            rendered.contains(&expected),
            "dense realization summary should keep {expected:?} visible:\n{rendered}"
        );
    }
    assert!(
        !rendered.contains("<|--"),
        "dense realization summary should not collapse dotted realization to solid inheritance:\n{rendered}"
    );
}