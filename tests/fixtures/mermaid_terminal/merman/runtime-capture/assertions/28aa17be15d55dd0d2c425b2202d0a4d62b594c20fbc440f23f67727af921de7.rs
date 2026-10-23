fn sequence_titles_render_above_participants() {
    let rendered = render_sequence(
        "sequenceDiagram\ntitle: Setup\nparticipant A\nparticipant B\nA->>B: Hi",
        &AsciiRenderOptions::ascii(),
    )
    .expect("sequence title should render");

    assert_eq!(
        rendered,
        concat!(
            "     Setup\n",
            "+---+     +---+\n",
            "| A |     | B |\n",
            "+-+-+     +-+-+\n",
            "  |         |\n",
            "  | Hi      |\n",
            "  +-------->|\n",
            "  |         |\n",
        )
    );

    let boxed = render_sequence(
        "sequenceDiagram\ntitle: Setup\nbox Group\nparticipant A\nparticipant B\nend\nA->>B: Hi",
        &AsciiRenderOptions::ascii(),
    )
    .expect("sequence title should render outside boxes");
    let mut boxed_lines = boxed.lines();
    assert_eq!(boxed_lines.next().unwrap().trim(), "Setup");
    assert!(
        boxed_lines.next().unwrap().starts_with("+- Group"),
        "title should stay above sequence boxes:\n{boxed}"
    );
}