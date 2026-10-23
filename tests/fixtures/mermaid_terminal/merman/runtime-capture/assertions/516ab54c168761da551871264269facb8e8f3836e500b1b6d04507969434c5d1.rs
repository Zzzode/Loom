fn er_parser_mixed_parallel_relationship_layout_preserves_all_facts_in_summary() {
    let rendered = render_er(
        "erDiagram\nA ||--|| B : a\nA ||..o{ B : b\nA ||--|| C : c",
        &AsciiRenderOptions::ascii(),
    )
    .expect("mixed parallel ER relationships should preserve every relationship");

    let a = framed_er_summary_relation("A", "||--||", "B", "a");
    let b = framed_er_summary_relation("A", "||..o{", "B", "b");
    let c = framed_er_summary_relation("A", "||--||", "C", "c");
    assert_eq!(
        rendered,
        format!(
            "+---+\n| A |\n+---+\n\n+---+\n| B |\n+---+\n\n+---+\n| C |\n+---+\n\nrelations:\n{a}\n{b}\n{c}\n"
        )
    );
}