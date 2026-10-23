fn class_parser_relation_fallback_obeys_the_operation_grid_budget() {
    let error = render_class_with_grid_limit(
        "classDiagram\nclass Gateway\nclass Service\nclass Repo\nGateway --> Service : routes<br>through\nService --> Repo : stores",
        &AsciiRenderOptions::ascii(),
        1,
    )
    .expect_err("a class fallback that cannot fit the operation budget must be rejected");

    assert!(matches!(error, AsciiError::ResourceLimitExceeded(_)));
}