fn direct_state_model_rejects_unknown_parent_ids() {
    let model = StateDiagramRenderModel {
        direction: "TB".to_string(),
        nodes: vec![direct_state_node("Child", "rect", Some("Missing"), None)],
        ..StateDiagramRenderModel::default()
    };

    let error = render_model(
        &RenderSemanticModel::State(model),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("unknown parent ids must not flatten state ownership");
    assert_eq!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "state",
            feature: "unknown state parent ids",
        }
    );
}