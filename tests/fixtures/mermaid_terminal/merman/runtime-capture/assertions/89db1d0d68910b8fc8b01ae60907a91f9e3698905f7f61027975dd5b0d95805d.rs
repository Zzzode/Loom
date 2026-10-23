fn er_parser_relationship_layout_propagates_grid_resource_errors() {
    let resources = AsciiResourcePolicy::default()
        .with_limit(AsciiResourceLimitId::MaxGridCells, 1)
        .expect("test resource limit should be valid");

    let error = render_er_with_resources(
        "erDiagram\nCUSTOMER\nORDER\nINVOICE\nCUSTOMER ||--o{ ORDER : \"places<br>orders\"\nORDER ||--|| INVOICE : bills",
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