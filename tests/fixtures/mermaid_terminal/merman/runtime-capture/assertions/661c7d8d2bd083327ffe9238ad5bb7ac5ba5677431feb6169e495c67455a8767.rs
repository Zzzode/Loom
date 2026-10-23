fn flowchart_parser_subgraph_reverse_directions_mirror_local_coordinates() {
    for (direction, expected_axis) in [("RL", "row"), ("BT", "column")] {
        let rendered = render_flowchart(
            &format!(
                "flowchart TB\nsubgraph reverse\n    direction {direction}\n    A --> B\nend\n"
            ),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap_or_else(|error| panic!("local {direction} subgraph should render: {error:?}"));

        let a = rendered
            .lines()
            .enumerate()
            .find_map(|(y, line)| line.find(" A ").map(|x| (x, y)))
            .unwrap_or_else(|| panic!("missing A in {direction} output:\n{rendered}"));
        let b = rendered
            .lines()
            .enumerate()
            .find_map(|(y, line)| line.find(" B ").map(|x| (x, y)))
            .unwrap_or_else(|| panic!("missing B in {direction} output:\n{rendered}"));

        if expected_axis == "row" {
            assert_eq!(
                a.1, b.1,
                "RL local direction should keep the edge horizontal:\n{rendered}"
            );
            assert!(
                b.0 < a.0,
                "RL local direction should place B before A:\n{rendered}"
            );
        } else {
            assert_eq!(
                a.0, b.0,
                "BT local direction should keep the edge vertical:\n{rendered}"
            );
            assert!(
                b.1 < a.1,
                "BT local direction should place B above A:\n{rendered}"
            );
        }
        assert_rectangular_char_grid(&rendered);
    }
}