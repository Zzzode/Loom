fn flowchart_parser_isolated_implicit_subgraph_uses_mermaid_perpendicular_default() {
    for (root_direction, expected_axis) in [
        ("TD", "row"),
        ("LR", "column"),
        ("BT", "column"),
        ("RL", "column"),
    ] {
        let rendered = render_flowchart(
            &format!("flowchart {root_direction}\nsubgraph isolated\n    A --> B\nend\n"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap_or_else(|error| {
            panic!("isolated subgraph under {root_direction} should render: {error:?}")
        });
        let position = |needle: &str| {
            rendered
                .lines()
                .enumerate()
                .find_map(|(y, line)| line.find(needle).map(|x| (x, y)))
                .unwrap_or_else(|| panic!("missing {needle:?} in output:\n{rendered}"))
        };
        let a = position(" A ");
        let b = position(" B ");

        if expected_axis == "row" {
            assert_eq!(
                a.1, b.1,
                "isolated subgraph should toggle {root_direction} to a horizontal axis:\n{rendered}"
            );
        } else {
            assert_eq!(
                a.0, b.0,
                "isolated subgraph should toggle {root_direction} to a vertical axis:\n{rendered}"
            );
        }
        assert_rectangular_char_grid(&rendered);
    }
}