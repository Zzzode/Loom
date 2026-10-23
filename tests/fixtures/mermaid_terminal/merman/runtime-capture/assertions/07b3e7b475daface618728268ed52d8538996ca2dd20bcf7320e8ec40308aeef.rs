fn sequence_long_message_labels_preserve_adjacent_and_intermediate_lifelines() {
    for mut options in [AsciiRenderOptions::unicode(), AsciiRenderOptions::ascii()] {
        for profile in [AsciiLayoutProfile::Canonical, AsciiLayoutProfile::Compact] {
            options.layout_profile = profile;
            for signal in ["A->>B", "B->>A", "A->>C", "C->>A"] {
                let source = format!(
                    "sequenceDiagram\nparticipant A\nparticipant B\nparticipant C\n{signal}: find user by email"
                );
                let rendered =
                    render_sequence(&source, &options).expect("long message must render");
                assert_label_lifelines(&rendered, "find user by email", &['A', 'B', 'C'], &options);
            }
        }
    }
}