fn class_parser_bidirectional_relationship_layout_preserves_both_directions_in_summary() {
    let rendered = render_class(
        "classDiagram\nclass A\nclass B\nA --> B : ab\nB --> A : ba",
        &AsciiRenderOptions::ascii(),
    )
    .expect("bidirectional class relationships should remain recoverable");

    let ab = framed_class_summary_relation("A", "-->", "B", Some("ab"));
    let ba = framed_class_summary_relation("B", "-->", "A", Some("ba"));
    assert_eq!(
        rendered,
        format!(
            concat!(
                "+---+\n",
                "| A |\n",
                "+---+\n",
                "\n",
                "+---+\n",
                "| B |\n",
                "+---+\n",
                "\n",
                "relations:\n",
                "{}\n",
                "{}\n",
            ),
            ab, ba,
        )
    );
}