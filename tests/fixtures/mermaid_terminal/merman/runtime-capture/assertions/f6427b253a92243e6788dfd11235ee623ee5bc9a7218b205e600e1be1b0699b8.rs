fn er_parser_horizontal_mixed_self_and_normal_relations_use_lossless_summary() {
    let rendered = render_er(
        concat!(
            "erDiagram\n",
            "direction LR\n",
            "A ||--o{ A : self\n",
            "A }o--|| B : next",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("mixed horizontal ER relations should remain recoverable");

    assert_eq!(rendered.matches("| A |").count(), 1, "{rendered}");
    assert_eq!(rendered.matches("| B |").count(), 1, "{rendered}");
    for expected in [
        "relations:".to_string(),
        framed_er_summary_relation("A", "||--o{", "A", "self"),
        framed_er_summary_relation("A", "}o--||", "B", "next"),
    ] {
        assert!(
            rendered.contains(&expected),
            "missing {expected:?}:\n{rendered}"
        );
    }
}