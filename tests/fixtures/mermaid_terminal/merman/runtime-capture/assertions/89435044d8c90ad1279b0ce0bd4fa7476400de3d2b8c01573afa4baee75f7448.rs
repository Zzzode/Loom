fn mindmap_render_model_rejects_parallel_edges() {
    let model = MindmapDiagramRenderModel {
        nodes: vec![
            mindmap_node("root", "Root", 0),
            mindmap_node("child", "Child", 1),
        ],
        edges: vec![
            mindmap_edge("e1", "root", "child"),
            mindmap_edge("e2", "root", "child"),
            mindmap_edge("e3", "child", "root"),
        ],
    };

    let error = render_model(
        &RenderSemanticModel::Mindmap(model),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("parallel mindmap edges must not be silently deduplicated");

    assert!(matches!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "mindmap",
            feature: "parallel edges",
        }
    ));
}