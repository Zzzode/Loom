fn flowchart_local_semantic_fixture_covers_sibling_group_boundary_routes() {
    let input = local_semantic_input("flowchart/sibling_boundary_routes.mmd");
    let rendered = render_flowchart(&input, &AsciiRenderOptions::ascii())
        .expect("local semantic sibling-boundary flowchart fixture should render");

    for expected in [
        "Left Group",
        "Right Group",
        "Alpha",
        "Beta",
        "Gamma",
        "Delta",
        "handoff",
    ] {
        assert!(
            rendered.contains(expected),
            "sibling-boundary flowchart fixture should keep {expected:?} visible:\n{rendered}"
        );
    }

    let line_index = |needle: &str| first_line_index_containing(&rendered, needle);

    assert!(
        line_index("Left Group") < line_index("Right Group"),
        "root TD direction should place the source sibling group before the target group:\n{rendered}"
    );
    assert!(
        line_index("Alpha") < line_index("Beta"),
        "source sibling group should preserve its internal TD chain:\n{rendered}"
    );
    assert!(
        line_index("Beta") < line_index("handoff"),
        "cross-boundary label should render after the source endpoint:\n{rendered}"
    );
    assert!(
        line_index("handoff") < line_index("Gamma"),
        "cross-boundary label should render before the target endpoint:\n{rendered}"
    );
    assert!(
        line_index("Gamma") < line_index("Delta"),
        "target sibling group should preserve its internal TD chain:\n{rendered}"
    );
}