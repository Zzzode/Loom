fn er_parser_parallel_relationship_layout_uses_lossless_summary_when_ports_do_not_fit() {
    let rendered = render_er(
        "erDiagram\nA ||--|| B : owns\nA ||..o{ B : contains",
        &AsciiRenderOptions::ascii(),
    )
    .expect("parallel ER relationships should preserve every relationship");

    let owns = framed_er_summary_relation("A", "||--||", "B", "owns");
    let contains = framed_er_summary_relation("A", "||..o{", "B", "contains");
    assert_eq!(
        rendered,
        format!("+---+\n| A |\n+---+\n\n+---+\n| B |\n+---+\n\nrelations:\n{owns}\n{contains}\n")
    );
}