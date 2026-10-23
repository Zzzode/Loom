fn auto_unsupported_family_and_canonical_route_errors_remain_errors() {
    let options = AsciiRenderOptions::ascii().with_layout_profile(AsciiLayoutProfile::Auto);
    let state = parse_model("stateDiagram-v2\n[*] --> Ready\nReady --> [*]");
    assert!(matches!(
        render_model_report(&state, &options, AsciiViewportPolicy::with_max_width(80)),
        Err(AsciiError::InvalidOption {
            field: "layout_profile",
            ..
        })
    ));

    let fanout = parse_model(
        "flowchart LR\nA -->|description 0| N0\nA -->|description 1| N1\nA -->|description 2| N2\nA -->|description 3| N3\nA -->|description 4| N4\nA -->|description 5| N5",
    );
    assert!(matches!(
        render_model_report(
            &fanout,
            &options,
            AsciiViewportPolicy::with_max_width(80).overflow(OverflowPolicy::Allow),
        ),
        Err(AsciiError::UnsupportedFeature { .. })
    ));
}