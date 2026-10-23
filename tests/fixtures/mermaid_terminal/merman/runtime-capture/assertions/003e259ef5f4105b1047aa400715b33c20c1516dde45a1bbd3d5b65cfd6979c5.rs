fn issue_53_exact_and_minus_one_grid_boundaries_remain_resource_outcomes() {
    let source = local_semantic_input("flowchart/issue_53_long_node_labels.mmd");
    let model = parse_model(&source);
    let options = AsciiRenderOptions::ascii();
    let unbounded = AsciiResourcePolicy::for_profile(ResourceProfile::UnboundedForTrustedInput);
    let rendered = render_model_with_resources(&model, &options, unbounded)
        .expect("unbounded Issue #53 fixture should render");
    let (width, height) = terminal_extent(&rendered);
    let exact_cells = width * height;
    assert!(
        exact_cells > 1,
        "fixture should consume more than one grid cell"
    );

    let exact_policy = unbounded
        .with_limit(AsciiResourceLimitId::MaxGridCells, exact_cells)
        .expect("exact grid limit should be valid");
    render_model_with_resources(&model, &options, exact_policy)
        .expect("exact Issue #53 grid boundary should remain accepted");

    let below_policy = unbounded
        .with_limit(AsciiResourceLimitId::MaxGridCells, exact_cells - 1)
        .expect("max-minus-one grid limit should be valid");
    let error = render_model_with_resources(&model, &options, below_policy)
        .expect_err("max-minus-one Issue #53 grid boundary should fail");
    let AsciiError::ResourceLimitExceeded(details) = error else {
        panic!("expected a grid resource error, got {error:?}");
    };
    assert_eq!(details.limit, AsciiResourceLimitId::MaxGridCells);
    assert_eq!(details.actual, exact_cells);
    assert_eq!(details.max, exact_cells - 1);
}