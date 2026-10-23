fn mindmap_uses_internal_ids_for_topology_and_authored_ids_for_disclosure() {
    let mut root = mindmap_node("0", "Root", 0);
    root.node_id = "authored-root".to_string();
    let mut child = mindmap_node("1", "Child", 1);
    child.node_id = "authored-child".to_string();
    let model = MindmapDiagramRenderModel {
        nodes: vec![root, child],
        edges: vec![mindmap_edge("edge_0_1", "0", "1")],
    };

    assert_eq!(
        render(RenderSemanticModel::Mindmap(model)),
        "node(bytes=4)=\"Root\" id(bytes=13)=\"authored-root\"\n\\-- node(bytes=5)=\"Child\" id(bytes=14)=\"authored-child\""
    );
}