fn er_parser_dense_crossing_relationships_fall_back_to_relation_summary() {
    let rendered = render_er(
        "erDiagram\nA ||--|| B : ab\nB ||--|| A : ba\nA ||--|| C : ac\nC ||--|| A : ca\nB ||--|| C : bc\nC ||--|| B : cb",
        &AsciiRenderOptions::ascii(),
    )
    .expect("dense ER relationships should render through relation summary fallback");

    let ab = framed_er_summary_relation("A", "||--||", "B", "ab");
    let ba = framed_er_summary_relation("B", "||--||", "A", "ba");
    let ac = framed_er_summary_relation("A", "||--||", "C", "ac");
    let ca = framed_er_summary_relation("C", "||--||", "A", "ca");
    let bc = framed_er_summary_relation("B", "||--||", "C", "bc");
    let cb = framed_er_summary_relation("C", "||--||", "B", "cb");
    assert_eq!(
        rendered,
        format!(
            "+---+\n| A |\n+---+\n\n+---+\n| B |\n+---+\n\n+---+\n| C |\n+---+\n\nrelations:\n{ab}\n{ba}\n{ac}\n{ca}\n{bc}\n{cb}\n"
        )
    );
}