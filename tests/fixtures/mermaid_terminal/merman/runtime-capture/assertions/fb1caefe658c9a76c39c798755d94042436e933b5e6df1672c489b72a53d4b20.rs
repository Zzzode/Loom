fn direct_state_model_rejects_non_group_and_cyclic_parents() {
    let non_group_model = StateDiagramRenderModel {
        direction: "TB".to_string(),
        nodes: vec![
            direct_state_node("Leaf", "rect", None, None),
            direct_state_node("Child", "rect", Some("Leaf"), None),
        ],
        ..StateDiagramRenderModel::default()
    };
    let error = render_model(
        &RenderSemanticModel::State(non_group_model),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("leaf states must not own child states");
    assert_eq!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "state",
            feature: "state parents that are not groups",
        }
    );

    let mut outer = direct_state_node("Outer", "roundedWithTitle", Some("Inner"), None);
    outer.is_group = true;
    outer.node_type = Some("group".to_string());
    let mut inner = direct_state_node("Inner", "roundedWithTitle", Some("Outer"), None);
    inner.is_group = true;
    inner.node_type = Some("group".to_string());
    let cyclic_model = StateDiagramRenderModel {
        direction: "TB".to_string(),
        nodes: vec![outer, inner],
        ..StateDiagramRenderModel::default()
    };
    let error = render_model(
        &RenderSemanticModel::State(cyclic_model),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("cyclic state ownership must not be projected");
    assert_eq!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "state",
            feature: "cyclic state parent ids",
        }
    );
}