fn class_parser_child_weighted_parent_order_keeps_readable_layout_routed() {
    let rendered = render_class(
        "classDiagram\nclass A\nclass B\nclass C\nclass D\nclass E\nA --> E : ae\nB --> D : bd\nC --> E : ce",
        &AsciiRenderOptions::ascii(),
    )
    .expect("child-weighted class topology should render");

    assert!(
        !rendered.contains("relations:"),
        "child-weighted class topology should use routed layout, not summary:\n{rendered}"
    );
    for expected in ["A", "B", "C", "D", "E", "ae", "bd", "ce"] {
        assert!(
            rendered.contains(expected),
            "routed child-weighted class topology should keep {expected:?} visible:\n{rendered}"
        );
    }
}