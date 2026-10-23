fn er_parser_spanning_level_relationship_layout_summarizes_invalid_outer_port() {
    let rendered = render_er(
        "erDiagram\nA ||--|| B : a\nB ||--|| C : b\nA ||--|| C : c",
        &AsciiRenderOptions::ascii(),
    )
    .expect("spanning-level ER relationships should remain recoverable");

    let a = framed_er_summary_relation("A", "||--||", "B", "a");
    let b = framed_er_summary_relation("B", "||--||", "C", "b");
    let c = framed_er_summary_relation("A", "||--||", "C", "c");
    assert_eq!(
        rendered,
        format!(
            "+---+\n| A |\n+---+\n\n+---+\n| B |\n+---+\n\n+---+\n| C |\n+---+\n\nrelations:\n{a}\n{b}\n{c}\n"
        )
    );
}