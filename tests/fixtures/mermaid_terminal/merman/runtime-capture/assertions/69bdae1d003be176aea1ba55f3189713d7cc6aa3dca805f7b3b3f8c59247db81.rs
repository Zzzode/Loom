fn same_id_group_style_preserves_flowdb_class_and_style_statement_order() {
    let render = |statements: &str| {
        let source = format!(
            "flowchart TD\nclassDef base stroke:#0000ff\nsubgraph G\n  A\nend\n{statements}\n"
        );
        let parsed = Engine::new()
            .parse_diagram_for_render_model_sync(&source, ParseOptions::strict())
            .expect("same-id group style order should parse")
            .expect("flowchart should be detected");
        render_parsed(
            &parsed,
            &AsciiRenderOptions::ascii().with_color_mode(AsciiColorMode::TrueColor),
        )
        .expect("same-id group style order should render")
    };

    let class_before_style_rendered = render("class G base\nstyle G fill:#ff0000");
    assert!(
        class_before_style_rendered.contains("\u{1b}[48;2;255;0;0m"),
        "vertex fill must replace the group CSS source:\n{class_before_style_rendered}"
    );
    assert!(
        !class_before_style_rendered.contains("\u{1b}[38;2;0;0;255m"),
        "a class assigned before vertex creation must not survive:\n{class_before_style_rendered}"
    );

    let style_before_class_rendered = render("style G fill:#ff0000\nclass G base");
    assert!(
        style_before_class_rendered.contains("\u{1b}[48;2;255;0;0m"),
        "vertex fill must remain visible:\n{style_before_class_rendered}"
    );
    assert!(
        style_before_class_rendered.contains("\u{1b}[38;2;0;0;255m"),
        "a class assigned after vertex creation must remain visible:\n{style_before_class_rendered}"
    );
}