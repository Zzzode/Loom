fn direct_state_model_rejects_non_string_label_values() {
    let mut node = direct_state_node("A", "rect", None, None);
    node.label = Some(false.into());
    let model = StateDiagramRenderModel {
        direction: "TB".to_string(),
        nodes: vec![node],
        ..StateDiagramRenderModel::default()
    };

    let error = render_model(
        &RenderSemanticModel::State(model),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("non-string labels must not be silently omitted");
    assert_eq!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "state",
            feature: "state labels with unsupported values",
        }
    );
}