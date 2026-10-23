fn compact_sequence_preserves_the_message_geometry_lower_bound() {
    let model = parse_model(
        "sequenceDiagram\nparticipant A\nparticipant B\nparticipant C\nA->>B: find user by email\nB->>C: validate session token",
    );
    for options in option_matrix() {
        let canonical =
            render_model(&model, &options).expect("canonical constrained messages must render");
        let compact = render_model(
            &model,
            &options.with_layout_profile(AsciiLayoutProfile::Compact),
        )
        .expect("compact constrained messages must render");
        assert_eq!(
            terminal_extent_with_profile(&canonical, options.terminal_width_profile),
            (51, 10)
        );
        assert_eq!(
            compact, canonical,
            "message clearance already fixes both intervals; Compact must preserve the same valid geometry"
        );
        assert_sequence_message_lifelines(&model, &compact, options.terminal_width_profile);
    }
}