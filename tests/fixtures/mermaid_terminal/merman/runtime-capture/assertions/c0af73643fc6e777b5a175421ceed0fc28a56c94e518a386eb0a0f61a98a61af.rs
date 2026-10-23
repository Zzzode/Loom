fn flowchart_parser_cross_subgraph_routes_follow_compound_parent_topology() {
    for (name, input, expected_nodes) in [
        (
            "directionless sibling groups",
            "flowchart LR\nsubgraph left\nA\nend\nsubgraph right\nB\nend\nA --> B",
            &["A", "B"][..],
        ),
        (
            "first parent for a repeated node reference",
            "flowchart TD\nsubgraph first\nA --> B\nend\nsubgraph second\nB --> C\nend",
            &["A", "B", "C"][..],
        ),
    ] {
        let rendered =
            render_flowchart(input, &AsciiRenderOptions::ascii()).unwrap_or_else(|error| {
                panic!("{name} should route across owned group borders: {error}")
            });

        for node in expected_nodes.iter().copied() {
            assert!(
                rendered.contains(node),
                "{name} should preserve node {node:?}:\n{rendered}"
            );
        }
        assert_rectangular_char_grid(&rendered);
    }
}