fn er_terminal_normalization_discloses_the_authored_box_identity() {
    let render_identity = |identity: &str| {
        let mut model = ErDiagramRenderModel::default();
        model.entities.insert(
            identity.to_string(),
            ErEntityRenderModel {
                id: "entity-0".to_string(),
                label: identity.to_string(),
                ..ErEntityRenderModel::default()
            },
        );
        render_er_model(&model, &AsciiRenderOptions::ascii())
            .expect("direct ER identity should render")
    };

    let control = render_identity("\u{1b}");
    let authored_escape = render_identity(r"\u{1B}");

    assert!(control.contains(r#"id(bytes=1)="\u{1B}""#), "{control}");
    assert_ne!(control, authored_escape);
}