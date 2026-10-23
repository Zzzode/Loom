fn er_parser_horizontal_parallel_self_relations_share_one_box() {
    let rendered = render_er(
        concat!(
            "erDiagram\n",
            "direction LR\n",
            "NODE ||--o{ NODE : children\n",
            "NODE |o..|{ NODE : optional",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("horizontal parallel ER self relations should render");

    assert_eq!(
        rendered.matches("| NODE |").count(),
        1,
        "parallel ER self relations must share one entity box:\n{rendered}"
    );
    for expected in ["children", "optional", "||", "o{", "|o", "|{"] {
        assert!(
            rendered.contains(expected),
            "parallel ER self relation must preserve {expected:?}:\n{rendered}"
        );
    }
}