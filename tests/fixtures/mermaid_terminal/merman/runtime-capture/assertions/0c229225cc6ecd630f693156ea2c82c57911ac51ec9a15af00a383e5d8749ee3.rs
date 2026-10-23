fn flowchart_wrapped_node_labels_respect_exact_grid_limit() {
    let input = "flowchart TD\nA[\"Alpha Beta Gamma Delta Epsilon\"] --> B[Done]";
    let unbounded = AsciiResourcePolicy::for_profile(ResourceProfile::UnboundedForTrustedInput);
    let base_options = AsciiRenderOptions::ascii().with_flowchart_node_label_wrap_width(8);
    let measured = render_flowchart_with_resources(input, &base_options, unbounded)
        .expect("unbounded graph should render");
    let exact_cells = rendered_terminal_width(&measured) * measured.lines().count();

    let exact_policy = unbounded
        .with_limit(AsciiResourceLimitId::MaxGridCells, exact_cells)
        .expect("exact grid limit should be valid");
    render_flowchart_with_resources(input, &base_options, exact_policy)
        .expect("exact wrapped-node grid limit should render");

    let below_policy = unbounded
        .with_limit(AsciiResourceLimitId::MaxGridCells, exact_cells - 1)
        .expect("max-minus-one grid limit should be valid");
    let error = render_flowchart_with_resources(input, &base_options, below_policy)
        .expect_err("max-minus-one wrapped-node grid limit should fail");
    let AsciiError::ResourceLimitExceeded(details) = error else {
        panic!("expected a grid resource error, got {error:?}");
    };
    assert_eq!(details.limit, AsciiResourceLimitId::MaxGridCells);
    assert_eq!(details.actual, exact_cells);
    assert_eq!(details.max, exact_cells - 1);
}