fn style_statement_on_a_colliding_vertex_remains_on_the_group_owner() {
    let source = concat!(
        "flowchart TD\n",
        "G\n",
        "subgraph G\n",
        "  A\n",
        "end\n",
        "style G fill:#ff0000\n",
        "G --> A\n",
    );
    let parsed = Engine::new()
        .parse_diagram_for_render_model_sync(source, ParseOptions::strict())
        .expect("styled node/subgraph collision should parse")
        .expect("flowchart should be detected");
    let rendered = render_parsed(
        &parsed,
        &AsciiRenderOptions::ascii().with_color_mode(AsciiColorMode::Html),
    )
    .expect("same-id vertex style should project through the group");

    assert!(
        rendered.contains("background-color:#ff0000"),
        "the vertex inline style must survive the group-first projection:\n{rendered}"
    );
}