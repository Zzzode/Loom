fn er_parser_multi_parent_relationship_layout_uses_barycenter_order() {
    let rendered = render_er(
        "erDiagram\nA ||--|| C : ac\nA ||--|| D : ad\nB ||--|| C : bc\nC ||--|| E : ce\nC ||--|| F : cf",
        &AsciiRenderOptions::ascii(),
    )
    .expect("multi-parent ER relationship layout should render");

    assert!(
        !rendered.contains("relations:"),
        "multi-parent ER topology should use routed layout, not summary:\n{rendered}"
    );
    for expected in ["ac", "ad", "bc", "ce", "cf"] {
        assert!(
            rendered.contains(expected),
            "routed multi-parent ER topology should keep {expected:?} visible:\n{rendered}"
        );
    }
}