fn er_parser_bidirectional_relationship_layout_preserves_both_directions_in_summary() {
    let rendered = render_er(
        "erDiagram\nA ||--|| B : ab\nB ||--|| A : ba",
        &AsciiRenderOptions::ascii(),
    )
    .expect("bidirectional ER relationships should remain recoverable");

    let ab = framed_er_summary_relation("A", "||--||", "B", "ab");
    let ba = framed_er_summary_relation("B", "||--||", "A", "ba");
    assert_eq!(
        rendered,
        format!("+---+\n| A |\n+---+\n\n+---+\n| B |\n+---+\n\nrelations:\n{ab}\n{ba}\n")
    );
}