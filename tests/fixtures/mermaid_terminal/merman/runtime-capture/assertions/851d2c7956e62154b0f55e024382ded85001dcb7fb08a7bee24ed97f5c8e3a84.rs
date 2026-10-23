fn flowchart_grid_admission_returns_structured_resource_error() {
    let model = parse_model("flowchart LR\n  A --> B\n");
    let error = render_controlled_model(
        &model,
        &AsciiRenderOptions::ascii(),
        &OperationControl::new(),
        &operation_context(),
        AsciiResourcePolicy::default()
            .with_limit(AsciiResourceLimitId::MaxGridCells, 1)
            .expect("one cell is a valid ASCII resource limit"),
    )
    .expect_err("the flowchart canvas should exceed one cell");

    assert!(matches!(
        error,
        AsciiError::ResourceLimitExceeded(resource)
            if resource.limit == AsciiResourceLimitId::MaxGridCells
                && resource.phase() == merman_ascii::AsciiResourceLimitPhase::Layout
                && resource.max == 1
                && resource.actual > resource.max
    ));
}