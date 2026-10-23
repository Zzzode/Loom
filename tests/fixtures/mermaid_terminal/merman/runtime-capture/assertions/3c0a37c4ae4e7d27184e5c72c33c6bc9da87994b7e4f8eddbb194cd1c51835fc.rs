fn flowchart_parser_self_loops_preserve_labels_in_all_directions() {
    for direction in ["TD", "BT", "LR", "RL"] {
        let rendered = render_flowchart(
            &format!("flowchart {direction}\nA -->|loop-{direction}| A"),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap_or_else(|error| panic!("{direction} self-loop should render: {error:?}"));

        assert!(rendered.contains('A'), "{direction}:\n{rendered}");
        assert!(
            rendered.contains(&format!("loop-{direction}")),
            "{direction} self-loop should preserve its label:\n{rendered}"
        );
        assert!(
            rendered
                .chars()
                .any(|ch| matches!(ch, '>' | '<' | '^' | 'v')),
            "{direction} self-loop should preserve its target marker:\n{rendered}"
        );
    }
}