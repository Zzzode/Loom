fn er_parser_relation_fallback_obeys_the_operation_grid_budget() {
    let error = render_er_with_grid_limit(
        "erDiagram\nCUSTOMER\nORDER\nINVOICE\nCUSTOMER ||--o{ ORDER : \"places<br>orders\"\nORDER ||--|| INVOICE : bills",
        &AsciiRenderOptions::ascii(),
        1,
    )
    .expect_err("an ER fallback that cannot fit the operation budget must be rejected");

    assert!(matches!(error, AsciiError::ResourceLimitExceeded(_)));
}