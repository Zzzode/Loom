fn kanban_structured_text_framing_distinguishes_ticket_from_priority() {
    let embedded_priority = KanbanDiagramRenderModel {
        nodes: vec![kanban_node(
            "card",
            "Card",
            false,
            KanbanNodeMetadata {
                ticket: Some("K-1, priority=high"),
                ..Default::default()
            },
        )],
    };
    let explicit_priority = KanbanDiagramRenderModel {
        nodes: vec![kanban_node(
            "card",
            "Card",
            false,
            KanbanNodeMetadata {
                ticket: Some("K-1"),
                priority: Some("high"),
                ..Default::default()
            },
        )],
    };

    assert_ne!(
        render(RenderSemanticModel::Kanban(embedded_priority)),
        render(RenderSemanticModel::Kanban(explicit_priority)),
        "authored ticket text must not be able to forge a priority field",
    );
}