fn kanban_render_model_rejects_duplicate_or_empty_ids() {
    let duplicate = KanbanDiagramRenderModel {
        nodes: vec![
            kanban_node("same", "A", true, KanbanNodeMetadata::default()),
            kanban_node("same", "B", false, KanbanNodeMetadata::default()),
        ],
    };
    let error = render_model(
        &RenderSemanticModel::Kanban(duplicate),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("group/card ids share one namespace");
    assert!(matches!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "kanban",
            feature: "duplicate node ids",
        }
    ));

    let empty = KanbanDiagramRenderModel {
        nodes: vec![kanban_node(
            "",
            "Empty",
            false,
            KanbanNodeMetadata::default(),
        )],
    };
    let error = render_model(
        &RenderSemanticModel::Kanban(empty),
        &AsciiRenderOptions::ascii(),
    )
    .expect_err("empty ids cannot provide stable card identity");
    assert!(matches!(
        error,
        AsciiError::UnsupportedFeature {
            diagram_type: "kanban",
            feature: "empty node ids",
        }
    ));
}