fn selected_graph_languages_render_independently() {
    let engine = Engine::new();
    let renderer = AsciiRenderer::new(AsciiRenderOptions::default()).unwrap();
    let context = merman_core::runtime::RuntimePolicy::deterministic()
        .begin_operation()
        .unwrap();
    for (enabled, source, family) in [
        (
            cfg!(feature = "diagram-flowchart"),
            "flowchart LR\nA[Start] --> B[Finish]\n",
            "flowchart",
        ),
        (
            cfg!(feature = "diagram-swimlane"),
            "swimlane-beta LR\nA[Start] --> B[Finish]\n",
            "swimlane",
        ),
        (
            cfg!(feature = "diagram-state"),
            "stateDiagram-v2\nStart --> Finish\n",
            "state",
        ),
    ] {
        if !enabled {
            continue;
        }
        let parsed = engine
            .parse_diagram_for_render_model_sync(source, ParseOptions::strict())
            .unwrap()
            .unwrap();
        let rendered = renderer
            .render_parsed_report(
                &parsed,
                merman_ascii::AsciiViewportPolicy::unrestricted(),
                &merman_core::OperationControl::new(),
                &context,
                AsciiResourcePolicy::default(),
            )
            .unwrap();
        assert!(rendered.text.contains("Start"), "{family}");
        assert!(rendered.text.contains("Finish"), "{family}");
    }
}