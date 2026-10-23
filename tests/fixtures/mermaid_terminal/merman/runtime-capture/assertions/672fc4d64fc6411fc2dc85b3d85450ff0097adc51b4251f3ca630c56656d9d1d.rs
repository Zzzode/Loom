fn er_parser_md_parent_cardinality_renders_explicit_marker() {
    let rendered = render_er(
        "erDiagram\nPROJECT u--o{ TEAM_MEMBER : parent",
        &AsciiRenderOptions::ascii(),
    )
    .expect("ER parent cardinality should render");

    for expected in ["PROJECT", "TEAM_MEMBER", "<>", "o{", "parent"] {
        assert!(
            rendered.contains(expected),
            "ER parent relationship should keep {expected:?} visible:\n{rendered}"
        );
    }

    let unicode = render_er(
        "erDiagram\nPROJECT u--o{ TEAM_MEMBER : parent",
        &AsciiRenderOptions::unicode(),
    )
    .expect("Unicode ER parent cardinality should render");
    assert!(
        unicode.contains('◆'),
        "Unicode ER parent cardinality should use a diamond marker:\n{unicode}"
    );
}