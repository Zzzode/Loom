fn flowchart_parser_self_loops_preserve_independent_source_and_target_markers() {
    for direction in ["TD", "BT", "LR", "RL"] {
        for (options, circle_marker, cross_marker) in [
            (AsciiRenderOptions::ascii(), 'o', 'x'),
            (AsciiRenderOptions::unicode(), '○', '×'),
        ] {
            let rendered = render_flowchart(&format!("flowchart {direction}\nA o--x A"), &options)
                .unwrap_or_else(|error| {
                    panic!("{direction} double-ended self-loop should render: {error:?}")
                });

            let circle = rendered
                .lines()
                .enumerate()
                .find_map(|(y, line)| line.find(circle_marker).map(|x| (x, y)))
                .unwrap_or_else(|| panic!("missing source circle marker:\n{rendered}"));
            let cross = rendered
                .lines()
                .enumerate()
                .find_map(|(y, line)| line.find(cross_marker).map(|x| (x, y)))
                .unwrap_or_else(|| panic!("missing target cross marker:\n{rendered}"));
            assert_ne!(
                circle, cross,
                "{direction} self-loop endpoint markers need independent berths:\n{rendered}"
            );
            assert_rectangular_char_grid(&rendered);
        }
    }
}