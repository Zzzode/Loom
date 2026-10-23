fn flowchart_node_label_plans_check_aggregate_text_limits() {
    let input = "flowchart TD\nA[Alpha]\nB[Bravo]";
    let unbounded = AsciiResourcePolicy::for_profile(ResourceProfile::UnboundedForTrustedInput);

    for limit in [
        AsciiResourceLimitId::MaxDocumentCells,
        AsciiResourceLimitId::MaxOutputBytes,
    ] {
        let policy = unbounded
            .with_limit(limit, 9)
            .expect("aggregate text limit should be valid");
        let error = render_flowchart_with_resources(input, &AsciiRenderOptions::ascii(), policy)
            .expect_err("two five-cell labels should exceed an aggregate limit of nine");
        let AsciiError::ResourceLimitExceeded(details) = error else {
            panic!("expected an aggregate text resource error, got {error:?}");
        };
        assert_eq!(details.limit, limit);
        assert_eq!(details.actual, 10);
        assert_eq!(details.max, 9);
    }
}