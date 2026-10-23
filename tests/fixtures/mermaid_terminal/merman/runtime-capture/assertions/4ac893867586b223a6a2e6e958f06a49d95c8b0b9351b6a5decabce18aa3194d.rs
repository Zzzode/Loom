fn class_parser_diagnostic_fallback_preserves_the_operation_resource_error() {
    let error = render_class_with_grid_limit(
        "classDiagram\nclass Gateway\nclass Service\nclass Repo\nGateway --> Service : routes\nService --> Repo : stores",
        &AsciiRenderOptions::ascii().with_relation_summary_diagnostics(true),
        1,
    )
    .expect_err("diagnostics must not bypass the operation grid budget");

    assert!(matches!(error, AsciiError::ResourceLimitExceeded(_)));
}