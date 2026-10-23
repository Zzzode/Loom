fn er_parser_horizontal_direction_propagates_resource_errors() {
    let input = "erDiagram\ndirection RL\nA ||--o{ B : owns";

    for limit in [
        AsciiResourceLimitId::MaxGridCells,
        AsciiResourceLimitId::MaxLayoutWorkUnits,
    ] {
        let resources = AsciiResourcePolicy::default()
            .with_limit(limit, 1)
            .expect("horizontal ER resource limit should be valid");
        let error = render_er_with_resources(input, &AsciiRenderOptions::ascii(), resources)
            .expect_err("horizontal ER rendering must propagate resource errors");
        assert!(matches!(
            error,
            AsciiError::ResourceLimitExceeded(details) if details.limit == limit
        ));
    }
}