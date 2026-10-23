fn direct_state_model_preserves_compartments_and_note_side_constraints() {
    let mut titled = direct_state_node("Primary", "rectWithTitle", None, None);
    titled.description = Some(vec!["Details".to_string()]);
    let model = StateDiagramRenderModel {
        direction: "TB".to_string(),
        nodes: vec![
            titled,
            direct_state_node("right direct", "noteGroup", None, Some("right of")),
            direct_state_node("right-note", "note", Some("right direct"), Some("right of")),
            direct_state_node("left direct", "noteGroup", None, Some("left of")),
            direct_state_node("left-note", "note", Some("left direct"), Some("left of")),
        ],
        edges: vec![
            StateDiagramRenderEdge {
                id: "right-edge".to_string(),
                start: "Primary".to_string(),
                end: "right-note".to_string(),
                classes: "transition note-edge".to_string(),
                arrow_type_end: String::new(),
                label: String::new(),
            },
            StateDiagramRenderEdge {
                id: "left-edge".to_string(),
                start: "left-note".to_string(),
                end: "Primary".to_string(),
                classes: "transition note-edge".to_string(),
                arrow_type_end: String::new(),
                label: String::new(),
            },
        ],
        ..StateDiagramRenderModel::default()
    };

    let rendered = render_model(
        &RenderSemanticModel::State(model),
        &AsciiRenderOptions::unicode(),
    )
    .expect("valid direct state model should render");

    let title = text_position(&rendered, "Primary");
    let body = text_position(&rendered, "Details");
    assert!(
        rendered
            .lines()
            .skip(title.1 + 1)
            .take(body.1.saturating_sub(title.1 + 1))
            .any(|line| line.contains('─')),
        "direct-model title/body roles need a structural divider:\n{rendered}"
    );

    let left = text_position(&rendered, "left direct");
    let state = text_position(&rendered, "Primary");
    let right = text_position(&rendered, "right direct");
    assert!(
        left.0 < state.0 && state.0 < right.0,
        "direct-model note side constraints must survive node reordering:\n{rendered}"
    );
}