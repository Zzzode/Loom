fn er_parser_diagnostic_fallback_preserves_the_operation_resource_error() {
    let error = render_er_with_grid_limit(
        "erDiagram\nCUSTOMER\nORDER\nINVOICE\nCUSTOMER ||--o{ ORDER : places\nORDER ||--|| INVOICE : bills",
        &AsciiRenderOptions::ascii().with_relation_summary_diagnostics(true),
        1,
    )
    .expect_err("diagnostics must not bypass the operation grid budget");

    assert!(matches!(error, AsciiError::ResourceLimitExceeded(_)));
}