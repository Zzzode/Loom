fn class_parser_dense_crossing_relationships_fall_back_to_relation_summary() {
    let rendered = render_class(
        "classDiagram\nclass A\nclass B\nclass C\nA --> B : ab\nB --> A : ba\nA --> C : ac\nC --> A : ca\nB --> C : bc\nC --> B : cb",
        &AsciiRenderOptions::ascii(),
    )
    .expect("dense class relationships should render through relation summary fallback");

    let ab = framed_class_summary_relation("A", "-->", "B", Some("ab"));
    let ba = framed_class_summary_relation("B", "-->", "A", Some("ba"));
    let ac = framed_class_summary_relation("A", "-->", "C", Some("ac"));
    let ca = framed_class_summary_relation("C", "-->", "A", Some("ca"));
    let bc = framed_class_summary_relation("B", "-->", "C", Some("bc"));
    let cb = framed_class_summary_relation("C", "-->", "B", Some("cb"));
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
                "+---+\n",
                "| C |\n",
                "+---+\n",
                "\n",
                "relations:\n",
                "{}\n",
                "{}\n",
                "{}\n",
                "{}\n",
                "{}\n",
                "{}\n",
            ),
            ab, ba, ac, ca, bc, cb,
        )
    );
}