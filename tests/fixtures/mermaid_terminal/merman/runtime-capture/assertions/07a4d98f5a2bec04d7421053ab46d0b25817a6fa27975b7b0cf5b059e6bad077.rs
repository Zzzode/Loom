fn auto_preserves_styled_encoding_and_rejects_styled_fallback_preflight() {
    let model = parse_model(LONG_NODE_CHAIN);
    for (color, encoded_marker) in [
        (AsciiColorMode::Ansi16, "\u{1b}["),
        (AsciiColorMode::Html, "<span"),
    ] {
        let options = AsciiRenderOptions::ascii()
            .with_layout_profile(AsciiLayoutProfile::Auto)
            .with_color_mode(color);
        let output = render_model_report(
            &model,
            &options,
            AsciiViewportPolicy::with_max_width(120).overflow(OverflowPolicy::Error),
        )
        .expect("automatic fitting supports styled primary output");
        assert_eq!(output.layout_profile, AsciiLayoutProfile::Compact);
        assert!(output.compact_attempted);
        assert!(output.text.contains(encoded_marker));
        assert!(output.emitted_extent.width <= 120);
        assert!(matches!(
            render_model_report(
                &model,
                &options,
                AsciiViewportPolicy::with_max_width(200).overflow(OverflowPolicy::Fallback),
            ),
            Err(AsciiError::InvalidOption {
                field: "ascii_viewport.overflow",
                ..
            })
        ));
    }
}