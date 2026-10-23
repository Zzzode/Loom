fn empty_er_direction_bytes_are_admitted_before_parsing() {
    let model = ErDiagramRenderModel {
        direction: format!("{}sideways", " ".repeat(1_024)),
        ..ErDiagramRenderModel::default()
    };
    let direction_bytes = model.direction.len();
    let model = RenderSemanticModel::Er(model);
    let resources = AsciiResourcePolicy::default()
        .with_limit(AsciiResourceLimitId::MaxLayoutWorkUnits, 1)
        .expect("ER direction work limit should be valid");

    let error = render_model_with_resources(&model, &AsciiRenderOptions::ascii(), resources)
        .expect_err("direction bytes must be admitted before empty-model validation");
    assert!(matches!(
        error,
        AsciiError::ResourceLimitExceeded(details)
            if details.limit == AsciiResourceLimitId::MaxLayoutWorkUnits
                && details.actual == direction_bytes
                && details.max == 1
    ));
}