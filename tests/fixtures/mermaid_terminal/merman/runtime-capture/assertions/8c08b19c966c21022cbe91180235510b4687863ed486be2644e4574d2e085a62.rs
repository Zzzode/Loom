fn sequence_local_control_frame_can_extend_beyond_a_partial_participant_box() {
    let rendered = render_sequence(
        concat!(
            "sequenceDiagram\n",
            "box Pair\n",
            "participant A\n",
            "participant B\n",
            "end\n",
            "participant C\n",
            "alt choose\n",
            "A->>C: yes\n",
            "else retry\n",
            "A->>C: no\n",
            "end",
        ),
        &AsciiRenderOptions::ascii(),
    )
    .expect("a local frame may extend beyond a partial participant box");

    assert!(
        rendered.contains("Pair"),
        "the partial box should render:\n{rendered}"
    );
    assert!(
        rendered.contains("alt choose"),
        "the local frame should render:\n{rendered}"
    );
    assert!(
        rendered.contains("yes"),
        "the first section should render:\n{rendered}"
    );
    assert!(
        rendered.contains("no"),
        "the second section should render:\n{rendered}"
    );
}