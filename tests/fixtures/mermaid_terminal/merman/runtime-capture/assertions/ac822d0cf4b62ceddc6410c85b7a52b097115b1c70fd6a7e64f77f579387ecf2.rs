fn class_parser_horizontal_direction_propagates_resource_errors() {
    let input = "classDiagram\ndirection LR\nclass A\nclass B\nA --> B";

    for limit in [
        AsciiResourceLimitId::MaxGridCells,
        AsciiResourceLimitId::MaxLayoutWorkUnits,
    ] {
        let resources = AsciiResourcePolicy::default()
            .with_limit(limit, 1)
            .expect("horizontal class resource limit should be valid");
        let error = render_class_with_resources(input, &AsciiRenderOptions::ascii(), resources)
            .expect_err("horizontal class rendering must propagate resource errors");
        assert!(matches!(
            error,
            AsciiError::ResourceLimitExceeded(details) if details.limit == limit
        ));
    }
}