fn kanban_group_parent_ownership_is_disclosed_without_nested_geometry() {
    let model = KanbanDiagramRenderModel {
        nodes: vec![
            kanban_node("root", "Root", true, KanbanNodeMetadata::default()),
            kanban_node(
                "child",
                "Child",
                true,
                KanbanNodeMetadata {
                    parent_id: Some("root, priority=high"),
                    ..Default::default()
                },
            ),
        ],
    };

    let rendered = render(RenderSemanticModel::Kanban(model));

    assert_eq!(
        rendered,
        concat!(
            "group(bytes=4)=\"Root\" [id(bytes=4)=\"root\"]\n",
            "group(bytes=5)=\"Child\" [id(bytes=5)=\"child\",\n",
            "parent(bytes=19)=\"root, priority=high\"]",
        )
    );
}