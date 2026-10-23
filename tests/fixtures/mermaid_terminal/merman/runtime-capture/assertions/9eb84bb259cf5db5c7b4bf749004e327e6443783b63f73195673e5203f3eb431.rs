fn er_parser_horizontal_shared_source_crossings_use_lossless_summary() {
    let rendered = render_er(
        concat!(
            "erDiagram\n",
            "direction LR\n",
            "A ||--|| B : short\n",
            "A ||--|| C : long",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("shared-source horizontal crossings should remain recoverable");

    assert!(rendered.contains("relations:"), "{rendered}");
    for expected in [
        framed_er_summary_relation("A", "||--||", "B", "short"),
        framed_er_summary_relation("A", "||--||", "C", "long"),
    ] {
        assert!(
            rendered.contains(&expected),
            "summary must preserve {expected:?} after shared-source crossing fallback:\n{rendered}"
        );
    }
}