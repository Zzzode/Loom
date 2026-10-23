fn sequence_explicit_wrap_uses_one_free_interval_and_retains_wrapping() {
    for signal in ["A->>C", "C->>A", "A->>A"] {
        let options = AsciiRenderOptions::unicode();
        let rendered = render_sequence(
            &format!("sequenceDiagram\nparticipant A\nparticipant B\nparticipant C\n{signal}: wrap:find user by email"),
            &options,
        ).expect("explicit wrap must remain supported");
        assert!(
            !rendered.contains("find user by email"),
            "wrap must not widen to natural label width:\n{rendered}"
        );
        let baseline = render_sequence(
            "sequenceDiagram\nparticipant A\nparticipant B\nparticipant C\nA->>C:",
            &options,
        )
        .expect("empty-label comparison must render");
        assert_eq!(
            actor_columns(&rendered, &['A', 'B', 'C']),
            actor_columns(&baseline, &['A', 'B', 'C']),
            "wrapped labels must retain the preferred participant spacing"
        );
        for word in ["find", "user", "by", "email"] {
            assert_label_lifelines(&rendered, word, &['A', 'B', 'C'], &options);
        }
    }
}