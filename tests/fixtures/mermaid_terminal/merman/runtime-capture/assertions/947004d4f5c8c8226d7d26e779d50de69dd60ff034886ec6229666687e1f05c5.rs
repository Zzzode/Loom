fn flowchart_node_label_wrapping_renders_for_shapes_and_root_directions() {
    for direction in ["TD", "LR", "RL", "BT"] {
        let input = format!(
            "flowchart {direction}\nA@{{ shape: stadium, label: \"Alpha Beta Gamma Delta\" }} --> B[Done]"
        );
        let rendered = render_flowchart(
            &input,
            &AsciiRenderOptions::unicode().with_flowchart_node_label_wrap_width(8),
        )
        .unwrap_or_else(|error| panic!("{direction} wrapped stadium should render: {error}"));

        for expected in ["Alpha", "Beta", "Gamma", "Delta", "Done"] {
            assert!(
                rendered.contains(expected),
                "missing {expected:?} for {direction}:\n{rendered}"
            );
        }
        assert_rectangular_terminal_grid(&rendered);
    }
}