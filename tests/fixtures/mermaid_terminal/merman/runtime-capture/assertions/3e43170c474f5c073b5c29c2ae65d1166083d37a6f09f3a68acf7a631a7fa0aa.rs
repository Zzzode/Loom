fn er_parser_horizontal_unrelated_edge_crossings_use_lossless_summary() {
    let rendered = render_er(
        concat!(
            "erDiagram\n",
            "direction LR\n",
            "A {\n  string id\n}\n",
            "B {\n  string id\n}\n",
            "C {\n  string id\n}\n",
            "D {\n  string id\n}\n",
            "A ||--|| C : first\n",
            "B }o..|| D : second\n",
            "A ||--|| B : bridge",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("unrelated horizontal crossings should remain recoverable");

    assert!(rendered.contains("relations:"), "{rendered}");
    for expected in [
        framed_er_summary_relation("A", "||--||", "C", "first"),
        framed_er_summary_relation("B", "}o..||", "D", "second"),
        framed_er_summary_relation("A", "||--||", "B", "bridge"),
    ] {
        assert!(
            rendered.contains(&expected),
            "summary must preserve {expected:?} after owner crossing fallback:\n{rendered}"
        );
    }
}