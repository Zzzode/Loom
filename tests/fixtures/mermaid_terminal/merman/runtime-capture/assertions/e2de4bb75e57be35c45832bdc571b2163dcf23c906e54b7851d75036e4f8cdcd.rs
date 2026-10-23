fn state_local_reverse_direction_preserves_physical_note_sides() {
    let rendered = render_state(
        concat!(
            "stateDiagram-v2\n",
            "direction TB\n",
            "state \"Outer\" as Outer {\n",
            "  direction RL\n",
            "  A\n",
            "  B\n",
            "  A --> B\n",
            "  note left of A : left local\n",
            "  note right of A : right local\n",
            "}\n",
        ),
        &AsciiRenderOptions::unicode(),
    )
    .expect("local reverse direction should preserve note ownership");

    let left = text_position(&rendered, "left local");
    let state = text_position(&rendered, " A ");
    let right = text_position(&rendered, "right local");
    assert!(
        left.0 < state.0 && state.0 < right.0,
        "local RL placement must not reverse physical note sides:\n{rendered}"
    );
}