fn sequence_rect_par_over_control_blocks_support_notes_activations_and_boxes() {
    let cases = [
        (
            "rect rgba(0,0,0,0.1)",
            "rect rgba(0,0,0,0.1)",
            "sequenceDiagram\nbox Group\nparticipant A\nparticipant B\nend\nrect rgba(0,0,0,0.1)\nNote over A,B: Wait\nA->>+B: Start\nB-->>-A: Done\nend",
        ),
        (
            "par_over Everyone",
            "par_over Everyone",
            "sequenceDiagram\nbox Group\nparticipant A\nparticipant B\nend\npar_over Everyone\nNote over A,B: Wait\nA->>+B: Start\nB-->>-A: Done\nend",
        ),
    ];

    for (label, frame_marker, input) in cases {
        let rendered = render_sequence(input, &AsciiRenderOptions::unicode())
            .unwrap_or_else(|err| panic!("{label} should render: {err}"));

        assert!(
            rendered.contains("Group"),
            "{label} should preserve participant box labels:\n{rendered}"
        );
        assert!(
            rendered.contains(frame_marker),
            "{label} should render the control frame:\n{rendered}"
        );
        assert!(
            rendered
                .lines()
                .any(|line| line.starts_with('│') && line.contains("Wait")),
            "{label} should keep notes inside the frame:\n{rendered}"
        );
        assert!(
            rendered
                .lines()
                .any(|line| line.starts_with('│') && line.contains('┃')),
            "{label} should keep active lifelines inside the frame:\n{rendered}"
        );
    }
}