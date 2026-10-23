fn sequence_sectioned_control_blocks_render_unicode_frames() {
    let cases = [
        (
            "sequenceDiagram\nparticipant A\nparticipant B\nalt Success\nA->>B: OK\nelse Failure\nB-->>A: Retry\nend",
            "alt",
            "Success",
            "else",
            "Failure",
            "OK",
            "Retry",
        ),
        (
            "sequenceDiagram\nparticipant A\nparticipant B\npar First\nA->>B: One\nand Second\nB-->>A: Two\nend",
            "par",
            "First",
            "and",
            "Second",
            "One",
            "Two",
        ),
        (
            "sequenceDiagram\nparticipant A\nparticipant B\ncritical Must lock\nA->>B: Lock\noption Timeout\nB-->>A: Backoff\nend",
            "critical",
            "Must lock",
            "option",
            "Timeout",
            "Lock",
            "Backoff",
        ),
    ];

    for (input, keyword, label, separator, separator_label, first, second) in cases {
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
                .any(|line| line.starts_with(&format!("├ {separator} {separator_label} "))),
            "{keyword} should render a labeled Unicode section separator:\n{rendered}"
        );
        assert!(
            rendered
                .lines()
                .any(|line| line.starts_with('│') && line.contains(first)),
            "{keyword} should keep first section rows inside the frame:\n{rendered}"
        );
        assert!(
            rendered
                .lines()
                .any(|line| line.starts_with('│') && line.contains(second)),
            "{keyword} should keep second section rows inside the frame:\n{rendered}"
        );
    }
}