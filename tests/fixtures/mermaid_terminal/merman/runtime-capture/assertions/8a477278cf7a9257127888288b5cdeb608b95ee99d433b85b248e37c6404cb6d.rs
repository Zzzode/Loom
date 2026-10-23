fn er_parser_horizontal_cardinalities_mirror_at_physical_ports() {
    for direction in ["LR", "RL"] {
        let rendered = render_er(
            &format!(
                "erDiagram\ndirection {direction}\nA }}o--o{{ B : \"relationship label that forces a wide lane\""
            ),
            &AsciiRenderOptions::ascii(),
        )
        .unwrap_or_else(|err| panic!("horizontal ER direction {direction} should render: {err}"));

        let (left_title, right_title) = if direction == "LR" {
            ("| A |", "| B |")
        } else {
            ("| B |", "| A |")
        };
        let (_, left_column) = line_and_column_containing(&rendered, left_title);
        let (_, right_column) = line_and_column_containing(&rendered, right_title);
        let connector = rendered
            .lines()
            .find(|line| line.contains("}o") && line.contains("o{"))
            .unwrap_or_else(|| {
                panic!("ER cardinalities must mirror at physical ports:\n{rendered}")
            });
        let between_boxes = &connector[left_column + left_title.len()..right_column];

        assert!(
            !between_boxes.contains(' '),
            "the ER connector must span the complete port-to-port gap:\n{rendered}"
        );
        assert!(
            rendered.contains("relationship label that forces a wide lane"),
            "the ER relationship label must remain attached to its routed lane:\n{rendered}"
        );
    }
}