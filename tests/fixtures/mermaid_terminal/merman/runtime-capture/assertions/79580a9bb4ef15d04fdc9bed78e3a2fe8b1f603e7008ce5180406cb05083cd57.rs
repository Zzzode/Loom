fn class_parser_relation_layout_propagates_grid_resource_errors() {
    let resources = AsciiResourcePolicy::default()
        .with_limit(AsciiResourceLimitId::MaxGridCells, 1)
        .expect("test resource limit should be valid");

    let error = render_class_with_resources(
        "classDiagram\nclass Gateway\nclass Service\nclass Repo\nGateway --> Service : routes<br>through\nService --> Repo : stores",
        &AsciiRenderOptions::ascii(),
        resources,
    )
    .expect_err("grid resource errors must not become summary fallback");
    assert!(matches!(
        error,
        AsciiError::ResourceLimitExceeded(details)
            if details.limit == AsciiResourceLimitId::MaxGridCells
    ));
}