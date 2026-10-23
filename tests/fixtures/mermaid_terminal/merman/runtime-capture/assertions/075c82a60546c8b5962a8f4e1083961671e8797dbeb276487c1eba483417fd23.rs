fn er_parser_k2_2_relationships_use_a_bounded_planar_layout() {
    let rendered = render_er(
        concat!(
            "erDiagram\n",
            "A ||--o{ C : ac\n",
            "A |{--|| D : ad\n",
            "B o|..|{ C : bc\n",
            "B ||--|| D : bd",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("bounded K2,2 ER relationships should render diagrammatically");

    assert!(
        !rendered.contains("relations:"),
        "a strict K2,2 component should use the bounded planar layout:\n{rendered}"
    );
    for expected in ["A", "B", "C", "D", "ac", "ad", "bc", "bd", "||", "o{", "|{"] {
        assert!(
            rendered.contains(expected),
            "bounded K2,2 output should retain {expected:?}:\n{rendered}"
        );
    }
}