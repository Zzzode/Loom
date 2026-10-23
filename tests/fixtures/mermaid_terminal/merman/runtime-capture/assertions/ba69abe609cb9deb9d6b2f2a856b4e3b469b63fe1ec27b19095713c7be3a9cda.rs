fn mindmap_structured_text_framing_distinguishes_labels_from_authored_ids() {
    let mut embedded_id = mindmap_node("0", "Root [id=x]", 0);
    embedded_id.node_id = "y".to_string();

    let mut explicit_id = mindmap_node("0", "Root", 0);
    explicit_id.node_id = "x] [id=y".to_string();

    assert_ne!(
        render(RenderSemanticModel::Mindmap(MindmapDiagramRenderModel {
            nodes: vec![embedded_id],
            edges: Vec::new(),
        })),
        render(RenderSemanticModel::Mindmap(MindmapDiagramRenderModel {
            nodes: vec![explicit_id],
            edges: Vec::new(),
        })),
        "authored labels must not be able to forge authored node-id disclosure",
    );
}