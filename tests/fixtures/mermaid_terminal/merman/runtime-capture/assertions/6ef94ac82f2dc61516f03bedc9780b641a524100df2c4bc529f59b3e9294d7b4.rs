fn sequence_local_control_frame_includes_note_gutters_in_both_charsets() {
    let input = concat!(
        "sequenceDiagram\n",
        "participant A\n",
        "participant B\n",
        "participant C\n",
        "A->>C: kickoff\n",
        "loop poll\n",
        "Note right of C: queued\n",
        "B->>C: check\n",
        "end",
    );

    for (options, top_left, top_right, lifeline) in [
        (AsciiRenderOptions::ascii(), '+', '+', '|'),
        (AsciiRenderOptions::unicode(), '┌', '┐', '│'),
    ] {
        let rendered = render_sequence(input, &options)
            .expect("a partial-span frame should reserve its note gutter");
        let frame_top = rendered
            .lines()
            .find(|line| line.contains("loop poll"))
            .unwrap_or_else(|| panic!("the local loop should render:\n{rendered}"));
        let frame_left = frame_top
            .find(top_left)
            .unwrap_or_else(|| panic!("the local frame should have a left border:\n{rendered}"));
        let frame_right = frame_top
            .rfind(top_right)
            .unwrap_or_else(|| panic!("the local frame should have a right border:\n{rendered}"));
        let unrelated_lifeline = frame_top
            .find(lifeline)
            .unwrap_or_else(|| panic!("A's unrelated lifeline should remain visible:\n{rendered}"));
        assert!(
            unrelated_lifeline < frame_left,
            "the local frame should not claim actor A:\n{rendered}"
        );

        let note_row = rendered
            .lines()
            .find(|line| line.contains("queued"))
            .unwrap_or_else(|| panic!("the note should render inside the frame:\n{rendered}"));
        let note_start = note_row
            .find("queued")
            .unwrap_or_else(|| panic!("the note text should have a stable row:\n{rendered}"));
        let note_end = note_start + "queued".len();
        assert!(
            frame_left < note_start && note_end <= frame_right,
            "the planned frame should include the right-side note gutter:\n{rendered}"
        );
    }
}