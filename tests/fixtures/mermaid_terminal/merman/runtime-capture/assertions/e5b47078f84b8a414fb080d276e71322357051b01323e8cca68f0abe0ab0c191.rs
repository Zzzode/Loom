fn custom_models_stay_non_renderable_and_cancellation_wins() {
    let mut engine = Engine::new();
    engine
        .diagram_registry_mut()
        .insert("sequence", |_, _, control| {
            control.checkpoint()?;
            Ok(Ok(serde_json::json!({"custom": true})))
        });
    let parsed = engine
        .parse_diagram_for_render_model_sync(
            "sequenceDiagram\nA->>B: Hello\n",
            ParseOptions::strict(),
        )
        .unwrap()
        .unwrap();
    let renderer = AsciiRenderer::new(AsciiRenderOptions::default()).unwrap();
    let context = merman_core::runtime::RuntimePolicy::deterministic()
        .begin_operation()
        .unwrap();
    let control = merman_core::OperationControl::new();
    assert!(matches!(
        renderer.render_parsed(&parsed, &control, &context, AsciiResourcePolicy::default()),
        Err(AsciiError::UnsupportedDiagram { .. })
    ));
    control.cancel();
    assert!(matches!(
        renderer.render_parsed(&parsed, &control, &context, AsciiResourcePolicy::default()),
        Err(AsciiError::Cancelled(_))
    ));
}