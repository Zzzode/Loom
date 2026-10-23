fn class_parser_final_relation_document_obeys_the_operation_grid_budget() {
    let error = render_class_with_grid_limit(
        "classDiagram\nclass A\nclass B\nclass C\nclass D\nA --> B : ab\nC --> D : cd",
        &AsciiRenderOptions::ascii(),
        1,
    )
    .expect_err("the final joined class document must obey the operation grid budget");

    assert!(matches!(error, AsciiError::ResourceLimitExceeded(_)));
}