fn er_parser_horizontal_self_relation_mirrors_source_cardinality() {
    let rendered = render_er(
        "erDiagram\ndirection LR\nNODE }o--|| NODE : owns",
        &AsciiRenderOptions::ascii(),
    )
    .expect("horizontal ER self relation should render");

    assert_eq!(rendered.matches("| NODE |").count(), 1, "{rendered}");
    assert!(rendered.contains("}o"), "{rendered}");
    assert!(rendered.contains("||"), "{rendered}");
    assert!(rendered.contains("owns"), "{rendered}");
}