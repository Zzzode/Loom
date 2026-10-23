fn flowchart_root_and_local_reverse_directions_compose_once() {
    for (root_direction, local_direction, expected_axis, target_before_source) in [
        ("RL", "RL", "row", true),
        ("RL", "LR", "row", false),
        ("BT", "BT", "column", true),
        ("BT", "TB", "column", false),
    ] {
        let rendered = render_flowchart(
            &format!(
                "flowchart {root_direction}\nsubgraph reverse\n    direction {local_direction}\n    A --> B\nend\n"
            ),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap_or_else(|error| {
            panic!(
                "root {root_direction} with local {local_direction} should render: {error:?}"
            )
        });

        let a = rendered
            .lines()
            .enumerate()
            .find_map(|(y, line)| line.find(" A ").map(|x| (x, y)))
            .unwrap_or_else(|| panic!("missing A in output:\n{rendered}"));
        let b = rendered
            .lines()
            .enumerate()
            .find_map(|(y, line)| line.find(" B ").map(|x| (x, y)))
            .unwrap_or_else(|| panic!("missing B in output:\n{rendered}"));

        if expected_axis == "row" {
            assert_eq!(
                a.1, b.1,
                "local direction must stay horizontal:\n{rendered}"
            );
            assert_eq!(
                b.0 < a.0,
                target_before_source,
                "root/local horizontal mirrors must compose exactly once:\n{rendered}"
            );
        } else {
            assert_eq!(a.0, b.0, "local direction must stay vertical:\n{rendered}");
            assert_eq!(
                b.1 < a.1,
                target_before_source,
                "root/local vertical mirrors must compose exactly once:\n{rendered}"
            );
        }
        assert_rectangular_char_grid(&rendered);
    }
}