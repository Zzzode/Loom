fn kanban_render_model_keeps_unassigned_and_unknown_parent_cards() {
    let model = KanbanDiagramRenderModel {
        nodes: vec![
            kanban_node("backlog", "Backlog", true, KanbanNodeMetadata::default()),
            kanban_node(
                "known",
                "Known",
                false,
                KanbanNodeMetadata {
                    parent_id: Some("backlog"),
                    ..Default::default()
                },
            ),
            kanban_node("loose", "Loose", false, KanbanNodeMetadata::default()),
            kanban_node(
                "unknown",
                "Unknown",
                false,
                KanbanNodeMetadata {
                    parent_id: Some("missing"),
                    ticket: Some("K-404"),
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
            "  - card(bytes=5)=\"Known\" [id(bytes=5)=\"known\"]\n",
            "Unassigned\n",
            "  - card(bytes=5)=\"Loose\" [id(bytes=5)=\"loose\"]\n",
            "  - card(bytes=7)=\"Unknown\" [id(bytes=7)=\"unknown\", parent(bytes=7)=\"missing\",\n",
            "    ticket(bytes=5)=\"K-404\"]",
        )
    );
}