fn sequence_rect_par_over_control_blocks_support_created_and_destroyed_actors() {
    let cases = [
        (
            "rect rgba(0,0,0,0.1)",
            "sequenceDiagram\nparticipant A\nparticipant B\nrect rgba(0,0,0,0.1)\ncreate participant C\nB->>C: Hello C\nC->>B: Still here\ndestroy C\nB--xC: Bye C\nend",
        ),
        (
            "par_over Everyone",
            "sequenceDiagram\nparticipant A\nparticipant B\npar_over Everyone\ncreate participant C\nB->>C: Hello C\nC->>B: Still here\ndestroy C\nB--xC: Bye C\nend",
        ),
    ];

    for (label, input) in cases {
        let rendered = render_sequence(input, &AsciiRenderOptions::unicode())
            .unwrap_or_else(|err| panic!("{label} should render: {err}"));

        assert!(
            rendered.contains(label),
            "{label} should render the control frame:\n{rendered}"
        );
        assert_control_text_between_borders(&rendered, label, "Hello C");
        assert_control_text_between_borders(&rendered, label, "Bye C");
    }
}