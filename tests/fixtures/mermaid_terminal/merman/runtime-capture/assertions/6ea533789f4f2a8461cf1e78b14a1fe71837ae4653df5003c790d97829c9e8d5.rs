fn er_parser_identifying_relationship_renders_multiline_label() {
    let rendered = render_er(
        "erDiagram\nCUSTOMER ||--o{ ORDER : \"north<br>south\"",
        &AsciiRenderOptions::ascii(),
    )
    .expect("ER should render");

    for expected in [
        "CUSTOMER",
        "ORDER",
        "north",
        "south",
        r#"authored(bytes=14)="north<br>south""#,
    ] {
        assert!(
            rendered.contains(expected),
            "missing {expected:?}:\n{rendered}"
        );
    }
}