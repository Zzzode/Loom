fn mindmap_direct_model_rejects_nodes_with_multiple_parents() {
    let model = MindmapDiagramRenderModel {
        nodes: vec![
            mindmap_node("root", "Root", 0),
            mindmap_node("left", "Left", 1),
            mindmap_node("right", "Right", 1),
            mindmap_node("shared", "Shared", 2),
        ],
        edges: vec![
            mindmap_edge("root-left", "root", "left"),
            mindmap_edge("root-right", "root", "right"),
            mindmap_edge("left-shared", "left", "shared"),
            mindmap_edge("right-shared", "right", "shared"),
        ],
    };

    let error = render_model(
        &RenderSemanticModel::Mindmap(model),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("a Mindmap child must not be expanded below multiple parents");

    assert!(matches!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "mindmap",
            feature: "nodes with multiple parents",
        }
    ));
}