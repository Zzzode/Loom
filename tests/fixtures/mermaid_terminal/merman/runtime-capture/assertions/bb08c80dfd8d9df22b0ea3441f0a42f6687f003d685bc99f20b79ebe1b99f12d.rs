fn empty_er_model_validates_direction_before_returning_empty() {
    let mut model = ErDiagramRenderModel {
        direction: "lr".to_string(),
        ..ErDiagramRenderModel::default()
    };
    assert_eq!(
        render_er_model(&model, &AsciiRenderOptions::ascii())
            .expect("a valid lowercase direction should permit an empty ER model"),
        ""
    );

    model.direction = "sideways".to_string();
    assert_eq!(
        render_er_model(&model, &AsciiRenderOptions::ascii())
            .expect_err("an empty ER model must not bypass direction validation"),
        AsciiError::UnsupportedFeature {
            diagram_type: "er",
            feature: "unknown ER diagram directions",
        }
    );
}