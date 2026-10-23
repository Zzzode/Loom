fn class_parser_cyclic_relationship_layout_summarizes_disconnected_back_edge() {
    let rendered = render_class(
        "classDiagram\nclass A\nclass B\nclass C\nA --> B : ab\nB --> C : bc\nC --> A : ca",
        &AsciiRenderOptions::ascii(),
    )
    .expect("cyclic class relationships should render");

    let ab = framed_class_summary_relation("A", "-->", "B", Some("ab"));
    let bc = framed_class_summary_relation("B", "-->", "C", Some("bc"));
    let ca = framed_class_summary_relation("C", "-->", "A", Some("ca"));
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
            ab, bc, ca,
        )
    );
}