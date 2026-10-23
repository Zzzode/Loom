fn er_parser_final_relation_document_obeys_the_operation_grid_budget() {
    let error = render_er_with_grid_limit(
        "erDiagram\nCUSTOMER ||--o{ ORDER : places\nINVOICE ||--|| PAYMENT : captures",
        &AsciiRenderOptions::ascii(),
        1,
    )
    .expect_err("the final joined ER document must obey the operation grid budget");

    assert!(matches!(error, AsciiError::ResourceLimitExceeded(_)));
}