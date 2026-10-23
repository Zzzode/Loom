fn class_parser_extension_relation_renders_multiline_label() {
    let rendered = render_class(
        "classDiagram\nclass Animal\nclass Dog\nAnimal <|-- Dog : north<br>south",
        &AsciiRenderOptions::ascii(),
    )
    .expect("class diagram should render");

    for expected in [
        "Animal",
        "Dog",
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