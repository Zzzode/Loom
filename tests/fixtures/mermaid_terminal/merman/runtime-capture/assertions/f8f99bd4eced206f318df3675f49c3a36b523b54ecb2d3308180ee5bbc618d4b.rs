fn direct_state_model_keeps_multiline_compartment_boundary_after_bt_mirroring() {
    let mut titled = direct_state_node("Titled", "rectWithTitle", None, None);
    titled.label = Some("Title one<br>Title two".into());
    titled.description = Some(vec!["Body one<br>Body two".to_string()]);
    let model = StateDiagramRenderModel {
        direction: "BT".to_string(),
        nodes: vec![titled],
        ..StateDiagramRenderModel::default()
    };

    let rendered = render_model(
        &RenderSemanticModel::State(model),
        &AsciiRenderOptions::unicode(),
    )
    .expect("BT state compartments should render");

    let title = text_position(&rendered, "Title two");
    let body = text_position(&rendered, "Body one");
    assert!(
        rendered
            .lines()
            .skip(title.1 + 1)
            .take(body.1.saturating_sub(title.1 + 1))
            .any(|line| line.contains('─')),
        "the mirrored divider must remain between multiline title and body roles:\n{rendered}"
    );
}