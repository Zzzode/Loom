fn class_parser_parallel_relationship_layout_keeps_diagram_when_ports_fit() {
    let rendered = render_class(
        concat!(
            "classDiagram\n",
            "class VeryWideParent\n",
            "class VeryWideChild\n",
            "VeryWideParent <|-- VeryWideChild : p\n",
            "VeryWideParent <|-- VeryWideChild : b",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("wide parallel class relationships should keep distinct lanes");

    assert!(!rendered.contains("relations:"), "{rendered}");
    assert_eq!(rendered.matches('^').count(), 2, "{rendered}");
    for label in ["p", "b"] {
        assert!(rendered.contains(label), "missing {label:?}:\n{rendered}");
    }
}