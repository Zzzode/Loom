fn flowchart_parser_three_parallel_edges_keep_distinct_labels_and_markers() {
    for direction in ["TD", "LR"] {
        let rendered = render_flowchart(
            &format!("flowchart {direction}\nA -->|first| B\nA -->|second| B\nA -->|third| B"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap_or_else(|error| panic!("{direction} parallel edges should render: {error:?}"));

        for label in ["first", "second", "third"] {
            assert_eq!(
                rendered.matches(label).count(),
                1,
                "{direction} should preserve the parallel label {label:?}:\n{rendered}"
            );
        }
        let marker_coords = rendered
            .lines()
            .enumerate()
            .flat_map(|(y, line)| {
                line.chars().enumerate().filter_map(move |(x, ch)| {
                    matches!(ch, '>' | '<' | '^' | 'v').then_some((x, y))
                })
            })
            .collect::<Vec<_>>();
        assert_eq!(
            marker_coords.len(),
            3,
            "{direction} should preserve one independently positioned target marker per parallel edge:\n{rendered}"
        );
        let mut unique_marker_coords = marker_coords.clone();
        unique_marker_coords.sort_unstable();
        unique_marker_coords.dedup();
        assert_eq!(
            unique_marker_coords.len(),
            3,
            "{direction} parallel markers should occupy independent coordinates:\n{rendered}"
        );
        let mut label_coords = ["first", "second", "third"]
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
            "{direction} parallel labels should occupy independent lane coordinates:\n{rendered}"
        );
    }
}