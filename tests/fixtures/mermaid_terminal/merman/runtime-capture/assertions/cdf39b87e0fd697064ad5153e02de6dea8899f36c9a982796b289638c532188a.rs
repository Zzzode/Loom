fn kanban_render_model_renders_groups_and_child_metadata() {
    let model = KanbanDiagramRenderModel {
        nodes: vec![
            kanban_node("backlog", "Backlog", true, KanbanNodeMetadata::default()),
            kanban_node(
                "card-a",
                "Ticket A",
                false,
                KanbanNodeMetadata {
                    parent_id: Some("backlog"),
                    ticket: Some("K-1"),
                    priority: Some("high"),
                    assigned: Some("alice"),
                    icon: Some("bug"),
                },
            ),
            kanban_node(
                "card-b",
                "Ticket B",
                false,
                KanbanNodeMetadata {
                    parent_id: Some("backlog"),
                    ticket: Some("K-2"),
                    ..Default::default()
                },
            ),
            kanban_node("doing", "Doing", true, KanbanNodeMetadata::default()),
            kanban_node(
                "card-c",
                "Ticket C",
                false,
                KanbanNodeMetadata {
                    parent_id: Some("doing"),
                    ticket: Some("K-3"),
                    ..Default::default()
                },
            ),
        ],
    };

    let rendered = render(RenderSemanticModel::Kanban(model));

    assert_eq!(
        rendered,
        concat!(
            "group(bytes=7)=\"Backlog\" [id(bytes=7)=\"backlog\"]\n",
            "  - card(bytes=8)=\"Ticket A\" [id(bytes=6)=\"card-a\", ticket(bytes=3)=\"K-1\",\n",
            "    priority(bytes=4)=\"high\", assigned(bytes=5)=\"alice\", icon(bytes=3)=\"bug\"]\n",
            "  - card(bytes=8)=\"Ticket B\" [id(bytes=6)=\"card-b\", ticket(bytes=3)=\"K-2\"]\n",
            "group(bytes=5)=\"Doing\" [id(bytes=5)=\"doing\"]\n",
            "  - card(bytes=8)=\"Ticket C\" [id(bytes=6)=\"card-c\", ticket(bytes=3)=\"K-3\"]",
        )
    );
}