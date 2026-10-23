fn er_parser_parallel_relationship_layout_keeps_diagram_when_ports_fit() {
    let rendered = render_er(
        concat!(
            "erDiagram\n",
            "VeryWideParent ||--|| VeryWideChild : p\n",
            "VeryWideParent ||..o{ VeryWideChild : b",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("wide parallel ER relationships should keep distinct lanes");

    assert!(!rendered.contains("relations:"), "{rendered}");
    for marker in ["||", "o{"] {
        assert!(rendered.contains(marker), "missing {marker:?}:\n{rendered}");
    }
    for label in ["p", "b"] {
        assert!(rendered.contains(label), "missing {label:?}:\n{rendered}");
    }
}