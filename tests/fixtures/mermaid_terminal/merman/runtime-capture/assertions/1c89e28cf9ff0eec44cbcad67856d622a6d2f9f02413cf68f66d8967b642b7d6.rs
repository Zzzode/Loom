fn er_parser_cyclic_relationship_layout_summarizes_disconnected_back_edge() {
    let rendered = render_er(
        "erDiagram\nA ||--|| B : owns\nB ||--|| C : owns\nC ||--|| A : owns",
        &AsciiRenderOptions::ascii(),
    )
    .expect("cyclic ER relationships should render");

    let ab = framed_er_summary_relation("A", "||--||", "B", "owns");
    let bc = framed_er_summary_relation("B", "||--||", "C", "owns");
    let ca = framed_er_summary_relation("C", "||--||", "A", "owns");
    assert_eq!(
        rendered,
        format!(
            "+---+\n| A |\n+---+\n\n+---+\n| B |\n+---+\n\n+---+\n| C |\n+---+\n\nrelations:\n{ab}\n{bc}\n{ca}\n"
        )
    );
}