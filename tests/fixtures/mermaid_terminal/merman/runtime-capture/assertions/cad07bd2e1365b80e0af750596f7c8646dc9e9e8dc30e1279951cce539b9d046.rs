fn er_parser_relation_summary_can_show_crossing_diagnostic() {
    let options = AsciiRenderOptions::ascii().with_relation_summary_diagnostics(true);

    let rendered = render_er(
        "erDiagram\nA ||--|| B : ab\nB ||--|| A : ba\nA ||--|| C : ac\nC ||--|| A : ca\nB ||--|| C : bc\nC ||--|| B : cb",
        &options,
    )
    .expect("ER crossing summary diagnostic should render");

    assert!(rendered.contains("relations:"), "{rendered}");
    assert!(rendered.contains("reason: crossing"), "{rendered}");
}