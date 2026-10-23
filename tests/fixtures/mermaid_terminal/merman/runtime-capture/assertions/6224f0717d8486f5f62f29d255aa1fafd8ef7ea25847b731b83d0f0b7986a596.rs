fn class_parser_self_relation_preserves_endpoint_labels_and_both_markers() {
    let rendered = render_class(
        "classDiagram\nclass Node\nNode \"source\" <|--|> \"target\" Node : recursive",
        &AsciiRenderOptions::ascii(),
    )
    .expect("two-sided self class relation should render");

    for expected in [
        "endpoint 1: source",
        "relation: recursive",
        "endpoint 2: target",
        "^",
        "v",
    ] {
        assert!(
            rendered.contains(expected),
            "self relation should preserve {expected:?} near its loop:\n{rendered}"
        );
    }
}