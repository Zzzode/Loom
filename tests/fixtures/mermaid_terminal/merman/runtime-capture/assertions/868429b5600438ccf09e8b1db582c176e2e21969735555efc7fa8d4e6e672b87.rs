fn mindmap_render_model_renders_rooted_outline() {
    let model = MindmapDiagramRenderModel {
        nodes: vec![
            mindmap_node("root", "Root", 0),
            mindmap_node("child1", "Child 1", 1),
            mindmap_node("child2", "Child 2", 1),
            mindmap_node("leaf", "Leaf", 2),
        ],
        edges: vec![
            mindmap_edge("e1", "root", "child1"),
            mindmap_edge("e2", "root", "child2"),
            mindmap_edge("e3", "child1", "leaf"),
        ],
    };

    let rendered = render(RenderSemanticModel::Mindmap(model));

    assert_eq!(
        rendered,
        concat!(
            "node(bytes=4)=\"Root\" id(bytes=4)=\"root\"\n",
            "|-- node(bytes=7)=\"Child 1\" id(bytes=6)=\"child1\"\n",
            "|   \\-- node(bytes=4)=\"Leaf\" id(bytes=4)=\"leaf\"\n",
            "\\-- node(bytes=7)=\"Child 2\" id(bytes=6)=\"child2\"",
        )
    );
}