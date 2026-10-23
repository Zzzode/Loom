fn class_parser_standalone_empty_namespace_remains_visible() {
    let rendered = render_class(
        "classDiagram\nnamespace Empty {\n}",
        &AsciiRenderOptions::ascii(),
    )
    .expect("standalone empty namespace should render");

    assert!(rendered.contains("Empty"), "{rendered}");
    assert!(!rendered.contains("relations:"), "{rendered}");
}