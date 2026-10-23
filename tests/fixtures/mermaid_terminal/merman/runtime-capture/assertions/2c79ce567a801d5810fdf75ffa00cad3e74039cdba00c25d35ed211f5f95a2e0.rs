fn class_parser_spanning_level_relationship_layout_summarizes_invalid_outer_port() {
    let rendered = render_class(
        "classDiagram\nclass A\nclass B\nclass C\nA <|-- B\nB <|-- C\nA <|-- C",
        &AsciiRenderOptions::ascii(),
    )
    .expect("spanning-level class relationships should remain recoverable");

    let ab = framed_class_summary_relation("A", "<|--", "B", None);
    let bc = framed_class_summary_relation("B", "<|--", "C", None);
    let ac = framed_class_summary_relation("A", "<|--", "C", None);
    assert_eq!(
        rendered,
        format!(
            concat!(
                "+---+\n| A |\n+---+\n\n",
                "+---+\n| B |\n+---+\n\n",
                "+---+\n| C |\n+---+\n\n",
                "relations:\n",
                "{}\n",
                "{}\n",
                "{}\n",
            ),
            ab, bc, ac,
        )
    );
}