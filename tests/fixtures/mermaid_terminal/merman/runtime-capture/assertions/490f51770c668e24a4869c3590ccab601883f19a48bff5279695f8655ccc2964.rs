fn flowchart_parser_parallel_self_loops_keep_independent_lanes_in_all_directions() {
    for direction in ["TD", "BT", "LR", "RL"] {
        let rendered = render_flowchart(
            &format!("flowchart {direction}\nA -->|alpha| A\nA -- beta --o A\nA -- gamma --x A"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap_or_else(|error| panic!("{direction} parallel self-loops should render: {error:?}"));

        for label in ["alpha", "beta", "gamma"] {
            assert_eq!(
                rendered.matches(label).count(),
                1,
                "{direction} should preserve self-loop label {label:?}:\n{rendered}"
            );
        }

        let marker_cells = rendered
            .lines()
            .enumerate()
            .flat_map(|(y, line)| {
                line.chars().enumerate().filter_map(move |(x, ch)| {
                    matches!(ch, '>' | '<' | '^' | 'v' | 'o' | 'x').then_some((x, y, ch))
                })
            })
            .collect::<Vec<_>>();
        assert_eq!(
            marker_cells.len(),
            3,
            "{direction} should retain exactly one marker per self-loop:\n{rendered}"
        );
        assert!(
            marker_cells.iter().any(|(_, _, ch)| *ch == 'o'),
            "{direction} should retain the circle marker:\n{rendered}"
        );
        assert!(
            marker_cells.iter().any(|(_, _, ch)| *ch == 'x'),
            "{direction} should retain the cross marker:\n{rendered}"
        );
        assert!(
            marker_cells
                .iter()
                .any(|(_, _, ch)| matches!(*ch, '>' | '<' | '^' | 'v')),
            "{direction} should retain the point marker:\n{rendered}"
        );
        let mut marker_coords = marker_cells
            .iter()
            .map(|(x, y, _)| (*x, *y))
            .collect::<Vec<_>>();
        marker_coords.sort_unstable();
        marker_coords.dedup();
        assert_eq!(
            marker_coords.len(),
            3,
            "{direction} self-loop markers must occupy independent coordinates:\n{rendered}"
        );

        let mut label_coords = ["alpha", "beta", "gamma"]
            .into_iter()
            .map(|label| {
                rendered
                    .lines()
                    .enumerate()
                    .find_map(|(y, line)| line.find(label).map(|x| (x, y)))
                    .unwrap_or_else(|| panic!("missing {label:?} in rendered fixture:\n{rendered}"))
            })
            .collect::<Vec<_>>();
        label_coords.sort_unstable();
        label_coords.dedup();
        assert_eq!(
            label_coords.len(),
            3,
            "{direction} self-loop labels must occupy independent lanes:\n{rendered}"
        );
        assert_rectangular_char_grid(&rendered);
    }
}