fn auto_width_matrix_keeps_complete_nodes_and_arrows_when_compact_is_taller() {
    let model = parse_model(LONG_NODE_CHAIN);
    for options in [AsciiRenderOptions::ascii(), AsciiRenderOptions::unicode()] {
        let canonical = render_model(&model, &options).expect("canonical chain should render");
        let compact = render_model(
            &model,
            &options.with_layout_profile(AsciiLayoutProfile::Compact),
        )
        .expect("compact chain should render");
        let canonical_extent =
            terminal_extent_with_profile(&canonical, options.terminal_width_profile);
        let compact_extent = terminal_extent_with_profile(&compact, options.terminal_width_profile);
        assert!(compact_extent.0 < canonical_extent.0);
        assert!(compact_extent.1 > canonical_extent.1);
        assert!(canonical_extent.0 > 120);
        assert!(compact_extent.0 <= 120 && compact_extent.0 > 100);

        for width in [60, 80, 100, 120] {
            let output = render_model_report(
                &model,
                &options.with_layout_profile(AsciiLayoutProfile::Auto),
                AsciiViewportPolicy::with_max_width(width),
            )
            .expect("Allow should emit the complete narrower candidate");
            assert_eq!(output.text, compact);
            assert_eq!(output.requested_layout_profile, AsciiLayoutProfile::Auto);
            assert_eq!(output.layout_profile, AsciiLayoutProfile::Compact);
            assert!(output.compact_attempted);
            assert!(!output.fallback.attempted);
            assert_eq!(output.primary_extent.width, compact_extent.0);
            assert_eq!(output.primary_extent.height, compact_extent.1);
            assert_eq!(output.emitted_extent, output.primary_extent);
            assert_eq!(output.overflowed, width < 120);
            assert_eq!(
                output.outcome,
                if width < 120 {
                    AsciiOutputOutcome::WideAllowed
                } else {
                    AsciiOutputOutcome::Primary
                }
            );
            for word in ["many", "words", "are", "too", "much", "sometime"] {
                assert_eq!(
                    output.text.matches(word).count(),
                    4,
                    "lost {word}: {}",
                    output.text
                );
            }
            let marker = if options == AsciiRenderOptions::ascii() {
                '>'
            } else {
                '►'
            };
            assert_eq!(output.text.matches(marker).count(), 3, "{}", output.text);
            assert_rectangular_terminal_grid_with_profile(
                &output.text,
                options.terminal_width_profile,
            );
        }
    }
}