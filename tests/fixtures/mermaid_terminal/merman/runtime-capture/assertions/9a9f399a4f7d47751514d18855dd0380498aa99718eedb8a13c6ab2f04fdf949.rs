fn class_direction_bytes_are_admitted_before_parsing() {
    let mut model = parse_class_model("classDiagram\nclass A");
    model.direction = format!("{}sideways", " ".repeat(1_024));
    let direction_bytes = model.direction.len();
    let model = RenderSemanticModel::Class(model);
    let resources = AsciiResourcePolicy::default()
        .with_limit(AsciiResourceLimitId::MaxLayoutWorkUnits, 1)
        .expect("class direction work limit should be valid");

    let error = render_model_with_resources(&model, &AsciiRenderOptions::ascii(), resources)
        .expect_err("direction bytes must be admitted before direction validation");
    assert!(matches!(
        error,
        AsciiError::ResourceLimitExceeded(details)
            if details.limit == AsciiResourceLimitId::MaxLayoutWorkUnits
                && details.actual == direction_bytes
                && details.max == 1
    ));
}