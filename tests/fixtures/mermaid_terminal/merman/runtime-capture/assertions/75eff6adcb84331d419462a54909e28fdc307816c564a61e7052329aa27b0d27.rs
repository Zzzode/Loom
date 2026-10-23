fn class_parser_relation_summary_can_show_crossing_diagnostic() {
    let options = AsciiRenderOptions::ascii().with_relation_summary_diagnostics(true);

    let rendered = render_class(
        "classDiagram\nclass A\nclass B\nclass C\nA --> B : ab\nB --> A : ba\nA --> C : ac\nC --> A : ca\nB --> C : bc\nC --> B : cb",
        &options,
    )
    .expect("class crossing summary diagnostic should render");

    assert!(rendered.contains("relations:"), "{rendered}");
    assert!(rendered.contains("reason: crossing"), "{rendered}");
}