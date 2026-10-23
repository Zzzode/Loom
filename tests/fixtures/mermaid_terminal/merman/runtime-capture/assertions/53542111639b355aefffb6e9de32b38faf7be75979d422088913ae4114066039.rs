fn er_parser_child_weighted_parent_order_keeps_readable_layout_routed() {
    let rendered = render_er(
        "erDiagram\nA ||--|| E : ae\nB ||--|| D : bd\nC ||--|| E : ce",
        &AsciiRenderOptions::ascii(),
    )
    .expect("child-weighted ER topology should render");

    assert!(
        !rendered.contains("relations:"),
        "child-weighted ER topology should use routed layout, not summary:\n{rendered}"
    );
    for expected in ["A", "B", "C", "D", "E", "ae", "bd", "ce"] {
        assert!(
            rendered.contains(expected),
            "routed child-weighted ER topology should keep {expected:?} visible:\n{rendered}"
        );
    }
}