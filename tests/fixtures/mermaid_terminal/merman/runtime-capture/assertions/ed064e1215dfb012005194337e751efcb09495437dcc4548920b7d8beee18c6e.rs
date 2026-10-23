fn explicit_family_overrides_survive_auto_and_equal_width_prefers_canonical() {
    let cases = [
        (
            LONG_NODE_CHAIN,
            AsciiRenderOptions::ascii()
                .with_graph_padding_x(5)
                .with_flowchart_node_label_wrap_width(40),
        ),
        (
            "sequenceDiagram\nAlice->>Bob: ping\nBob-->>Alice: pong",
            AsciiRenderOptions::ascii().with_sequence_participant_spacing(5),
        ),
    ];
    for (source, options) in cases {
        let model = parse_model(source);
        let canonical = render_model(&model, &options).expect("explicit overrides should render");
        let compact = render_model(
            &model,
            &options.with_layout_profile(AsciiLayoutProfile::Compact),
        )
        .expect("Compact should preserve explicit overrides, including old default values");
        assert_eq!(canonical, compact);
        let width = terminal_extent_with_profile(&canonical, options.terminal_width_profile).0;
        let output = render_model_report(
            &model,
            &options.with_layout_profile(AsciiLayoutProfile::Auto),
            AsciiViewportPolicy::with_max_width(width - 1),
        )
        .expect("Auto should preserve a complete canonical candidate on a width tie");
        assert_eq!(output.text, canonical);
        assert_eq!(output.layout_profile, AsciiLayoutProfile::Canonical);
        assert!(output.compact_attempted);
        assert!(output.overflowed);
    }
}