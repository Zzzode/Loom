fn direct_state_model_rejects_edges_with_missing_endpoint_nodes() {
    let model = StateDiagramRenderModel {
        direction: "TB".to_string(),
        nodes: vec![direct_state_node("A", "rect", None, None)],
        edges: vec![StateDiagramRenderEdge {
            id: "missing-target".to_string(),
            start: "A".to_string(),
            end: "Missing".to_string(),
            classes: "transition".to_string(),
            arrow_type_end: "arrow_barb".to_string(),
            label: String::new(),
        }],
        ..StateDiagramRenderModel::default()
    };

    let error = render_model(
        &RenderSemanticModel::State(model),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("state edges must reference declared nodes before routing");
    assert_eq!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "state",
            feature: "edges with missing endpoint nodes",
        }
    );
}