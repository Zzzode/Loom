fn class_parser_parallel_relationship_layout_uses_lossless_summary_when_ports_do_not_fit() {
    let rendered = render_class(
        "classDiagram\nclass Animal\nclass Dog\nAnimal <|-- Dog : parent\nAnimal <|-- Dog : base",
        &AsciiRenderOptions::ascii(),
    )
    .expect("parallel class relationships should preserve every relation");

    let parent = framed_class_summary_relation("Animal", "<|--", "Dog", Some("parent"));
    let base = framed_class_summary_relation("Animal", "<|--", "Dog", Some("base"));
    assert_eq!(
        rendered,
        format!(
            concat!(
                "+--------+\n",
                "| Animal |\n",
                "+--------+\n",
                "\n",
                "+-----+\n",
                "| Dog |\n",
                "+-----+\n",
                "\n",
                "relations:\n",
                "{}\n",
                "{}\n",
            ),
            parent, base,
        )
    );
}