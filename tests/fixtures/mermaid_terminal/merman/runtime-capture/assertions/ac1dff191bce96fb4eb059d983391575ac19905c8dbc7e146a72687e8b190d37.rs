fn sequence_single_section_control_blocks_render_unicode_frames() {
    let cases = [
        (
            "sequenceDiagram\nparticipant A\nparticipant B\nloop Every minute\nA->>B: Ping\nend",
            "loop",
            "Every minute",
            "Ping",
        ),
        (
            "sequenceDiagram\nparticipant A\nparticipant B\nopt A is ready\nA->>B: Send\nend",
            "opt",
            "A is ready",
            "Send",
        ),
        (
            "sequenceDiagram\nparticipant A\nparticipant B\nbreak Failure\nA->>B: Stop\nend",
            "break",
            "Failure",
            "Stop",
        ),
    ];

    for (input, keyword, label, message_label) in cases {
        let rendered = render_sequence(input, &AsciiRenderOptions::unicode())
            .unwrap_or_else(|err| panic!("{keyword} should render: {err}"));

        assert!(
            rendered
                .lines()
                .any(|line| line.starts_with(&format!("┌ {keyword} {label} "))),
            "{keyword} should render a labeled Unicode top frame:\n{rendered}"
        );
        assert!(
            rendered
                .lines()
                .any(|line| line.starts_with('│') && line.contains(message_label)),
            "{keyword} should keep contained rows inside the Unicode frame:\n{rendered}"
        );
        assert!(
            rendered.lines().any(|line| line.starts_with('└')),
            "{keyword} should render a Unicode bottom frame:\n{rendered}"
        );
    }
}