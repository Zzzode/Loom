fn er_bottom_up_summary_preserves_semantic_endpoint_roles() {
    let rendered = render_er(
        concat!(
            "erDiagram\n",
            "direction BT\n",
            "A ||--o{ B : first\n",
            "A ||--o{ B : second",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("bottom-up parallel ER relationships should preserve semantic roles");

    let b_line = rendered
        .lines()
        .position(|line| line == "| B |")
        .expect("bottom-up summary should render entity B");
    let a_line = rendered
        .lines()
        .position(|line| line == "| A |")
        .expect("bottom-up summary should render entity A");
    assert!(b_line < a_line, "BT should place B above A:\n{rendered}");

    let summary = rendered
        .split_once("relations:\n")
        .expect("parallel relationships should use structured fallback")
        .1;
    for label in ["first", "second"] {
        let line = summary
            .lines()
            .find(|line| line.ends_with(label))
            .unwrap_or_else(|| panic!("missing {label:?} summary row:\n{rendered}"));
        assert!(line.starts_with(&framed_er_summary_endpoint("A")), "{line}");
        assert!(line.contains("||--o{"), "{line}");
        assert!(line.contains(&framed_er_summary_endpoint("B")), "{line}");
    }
}