fn direct_state_model_rejects_duplicate_node_ids_before_graph_projection() {
    let mut first = direct_state_node("A", "rect", None, None);
    first.label = Some("First A".into());
    let mut second = direct_state_node("A", "rect", None, None);
    second.label = Some("Second A".into());
    let model = StateDiagramRenderModel {
        direction: "TB".to_string(),
        nodes: vec![first, second],
        edges: vec![StateDiagramRenderEdge {
            id: "self".to_string(),
            start: "A".to_string(),
            end: "A".to_string(),
            classes: "transition".to_string(),
            arrow_type_end: "arrow_barb".to_string(),
            label: "ambiguous".to_string(),
        }],
        ..StateDiagramRenderModel::default()
    };

    let error = render_model(
        &RenderSemanticModel::State(model),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("duplicate node ids must not select different rank and route instances");
    assert_eq!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "state",
            feature: "duplicate node ids",
        }
    );
}