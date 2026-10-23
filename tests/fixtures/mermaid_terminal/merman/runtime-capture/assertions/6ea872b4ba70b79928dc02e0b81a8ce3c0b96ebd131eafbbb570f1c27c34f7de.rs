fn state_note_sides_remain_physical_across_root_directions() {
    for direction in ["TB", "BT", "LR", "RL"] {
        let rendered = render_state(
            &format!(
                concat!(
                    "stateDiagram-v2\n",
                    "direction {direction}\n",
                    "A\n",
                    "note left of A : left side\n",
                    "note right of A : right side\n",
                ),
                direction = direction,
            ),
            &AsciiRenderOptions::unicode(),
        )
        .unwrap_or_else(|error| panic!("{direction} state notes should render: {error}"));

        let left = text_position(&rendered, "left side");
        let state = text_position(&rendered, " A ");
        let right = text_position(&rendered, "right side");
        assert!(
            left.0 < state.0 && state.0 < right.0,
            "{direction} must preserve physical note sides after transforms:\n{rendered}"
        );
    }
}