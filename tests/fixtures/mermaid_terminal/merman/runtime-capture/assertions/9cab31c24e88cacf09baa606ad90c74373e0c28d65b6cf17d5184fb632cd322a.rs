fn class_bottom_up_summary_preserves_semantic_endpoint_roles() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "direction BT\n",
            "class A\n",
            "class B\n",
            "A \"source\" <|--|> \"target\" B : first\n",
            "A \"source2\" <|--|> \"target2\" B : second",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("bottom-up parallel class relationships should preserve semantic roles");

    let b_line = rendered
        .lines()
        .position(|line| line == "| B |")
        .expect("bottom-up summary should render class B");
    let a_line = rendered
        .lines()
        .position(|line| line == "| A |")
        .expect("bottom-up summary should render class A");
    assert!(b_line < a_line, "BT should place B above A:\n{rendered}");

    let summary = rendered
        .split_once("relations:\n")
        .expect("parallel relationships should use structured fallback")
        .1;
    for (source_label, target_label) in [("source", "target"), ("source2", "target2")] {
        let line = summary
            .lines()
            .find(|line| line.contains(source_label))
            .unwrap_or_else(|| panic!("missing {source_label:?} summary row:\n{rendered}"));
        assert!(
            line.starts_with(&framed_class_summary_endpoint("A")),
            "{line}"
        );
        assert!(line.contains(&framed_class_summary_endpoint("B")), "{line}");
        assert!(line.contains("<|--|>"), "{line}");
        assert!(
            line.contains("endpoint1=[") && line.contains(source_label),
            "{line}"
        );
        assert!(
            line.contains("endpoint2=[") && line.contains(target_label),
            "{line}"
        );
    }
}