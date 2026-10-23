fn sequence_three_level_control_renders_with_a_leftmost_note() {
    let rendered = render_sequence(
        concat!(
            "sequenceDiagram\n",
            "participant A\n",
            "participant B\n",
            "participant C\n",
            "loop Outer\n",
            "alt Choice\n",
            "opt Inner\n",
            "Note left of A: queued\n",
            "A->>B: Work\n",
            "end\n",
            "end\n",
            "end",
        ),
        &AsciiRenderOptions::unicode(),
    )
    .expect("three nested controls with a leftmost note should render");

    for title in ["loop Outer", "alt Choice", "opt Inner"] {
        assert!(
            rendered.contains(title),
            "the {title:?} frame should remain visible:\n{rendered}"
        );
    }
    assert!(
        rendered.contains("queued"),
        "the leftmost note should remain visible:\n{rendered}"
    );
    assert!(
        rendered.contains("Work"),
        "the message following the nested note should remain visible:\n{rendered}"
    );
}