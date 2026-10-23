fn state_composite_notes_anchor_outside_the_group_boundary() {
    for direction in ["TB", "BT", "LR", "RL"] {
        let rendered = render_state(
            &format!(
                concat!(
                    "stateDiagram-v2\n",
                    "direction {direction}\n",
                    "state \"Composite\" as Parent {{\n",
                    "  Child\n",
                    "}}\n",
                    "note left of Parent : left composite\n",
                    "note right of Parent : right composite\n",
                ),
                direction = direction,
            ),
            &AsciiRenderOptions::unicode(),
        )
        .unwrap_or_else(|error| panic!("{direction} composite notes should render: {error}"));

        let left = text_position(&rendered, "left composite");
        let group = text_position(&rendered, "Composite");
        let right = text_position(&rendered, "right composite");
        assert!(
            left.0 < group.0 && group.0 < right.0,
            "{direction} composite note constraints should preserve both physical sides:\n{rendered}"
        );
    }
}