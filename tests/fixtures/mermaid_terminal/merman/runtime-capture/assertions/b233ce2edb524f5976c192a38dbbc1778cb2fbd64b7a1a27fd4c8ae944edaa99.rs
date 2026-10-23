fn sequence_long_self_messages_preserve_next_actor_and_loop_clearance() {
    for options in [AsciiRenderOptions::unicode(), AsciiRenderOptions::ascii()] {
        let rendered = render_sequence(
            "sequenceDiagram\nparticipant A\nparticipant B\nA->>A: find user by email\nB->>B: last actor message",
            &options,
        ).expect("self messages must have a label host");
        for label in ["find user by email", "last actor message"] {
            assert_label_lifelines(&rendered, label, &['A', 'B'], &options);
        }
        let center = actor_columns(&rendered, &['A', 'B'])[1];
        let rows = rendered.lines().collect::<Vec<_>>();
        let label_row = first_line_index_containing(&rendered, "find user by email");
        let lifeline = if options.charset == merman_ascii::AsciiCharset::Unicode {
            '│'
        } else {
            '|'
        };
        for row in &rows[label_row + 1..label_row + 4] {
            assert_eq!(
                glyph_at_column(row, center, options.terminal_width_profile),
                Some(lifeline),
                "a neighbouring actor must remain outside the self loop:\n{rendered}"
            );
        }
    }
}