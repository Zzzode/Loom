fn class_parser_two_sided_markers_keep_cardinalities_and_label() {
    let rendered = render_class(
        "classDiagram\nA \"1\" <|--|> \"*\" B : inherits both ways",
        &AsciiRenderOptions::ascii(),
    )
    .expect("two-sided class relationship markers should render");

    for expected in ["A", "B", "1", "*", "inherits both ways", "^", "v"] {
        assert!(
            rendered.contains(expected),
            "two-sided relationship should keep {expected:?} visible:\n{rendered}"
        );
    }

    let composition = render_class(
        "classDiagram\nWhole *--* Part",
        &AsciiRenderOptions::ascii(),
    )
    .expect("two-sided composition should render");
    assert!(
        composition.matches('*').count() >= 2,
        "both composition terminals should remain visible:\n{composition}"
    );
}