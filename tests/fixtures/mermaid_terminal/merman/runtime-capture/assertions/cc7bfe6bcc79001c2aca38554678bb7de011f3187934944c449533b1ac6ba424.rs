fn mindmap_render_model_keeps_disconnected_cycle_components() {
    let model = MindmapDiagramRenderModel {
        nodes: vec![
            mindmap_node("root", "Root", 0),
            mindmap_node("child", "Child", 1),
            mindmap_node("a", "A", 0),
            mindmap_node("b", "B", 1),
        ],
        edges: vec![
            mindmap_edge("e1", "root", "child"),
            mindmap_edge("e2", "a", "b"),
            mindmap_edge("e3", "b", "a"),
        ],
    };

    let rendered = render(RenderSemanticModel::Mindmap(model));

    assert_eq!(
        rendered,
        concat!(
            "node(bytes=4)=\"Root\" id(bytes=4)=\"root\"\n",
            "\\-- node(bytes=5)=\"Child\" id(bytes=5)=\"child\"\n",
            "\n",
            "node(bytes=1)=\"A\" id(bytes=1)=\"a\"\n",
            "\\-- node(bytes=1)=\"B\" id(bytes=1)=\"b\"\n",
            "    \\-- node(bytes=1)=\"A\" id(bytes=1)=\"a\" (cycle)",
        )
    );
}