fn git_graph_metadata_and_model_identities_share_one_capability() {
    assert!(merman_ascii::ascii_supported_diagram_types().contains(&"gitgraph"));
    let engine = Engine::new();
    let parsed = engine
        .parse_diagram_for_render_model_sync(
            "gitGraph\ncommit id:\"first\"\n",
            ParseOptions::strict(),
        )
        .unwrap()
        .unwrap();
    let renderer = AsciiRenderer::default();
    let context = engine.begin_operation().unwrap();
    let control = merman_core::OperationControl::new();
    let resources = AsciiResourcePolicy::default();
    assert!(
        renderer
            .render_parsed(&parsed, &control, &context, resources)
            .unwrap()
            .contains("first")
    );
    assert!(
        renderer
            .render_model(parsed.model(), &control, &context, resources)
            .unwrap()
            .contains("first")
    );
}