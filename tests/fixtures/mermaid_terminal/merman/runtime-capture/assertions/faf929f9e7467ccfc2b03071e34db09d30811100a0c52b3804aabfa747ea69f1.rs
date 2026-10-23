fn auto_keeps_canonical_without_retry_when_it_fits_exactly() {
    let model = parse_model("flowchart LR\nA -->|approved| B");
    for profile in [TerminalWidthProfile::Unicode, TerminalWidthProfile::Cjk] {
        let options = AsciiRenderOptions::unicode().with_terminal_width_profile(profile);
        let canonical = render_model(&model, &options).expect("canonical graph should render");
        let width = terminal_extent_with_profile(&canonical, profile).0;
        for overflow in [
            OverflowPolicy::Allow,
            OverflowPolicy::Error,
            OverflowPolicy::Fallback,
        ] {
            let output = render_model_report(
                &model,
                &options.with_layout_profile(AsciiLayoutProfile::Auto),
                AsciiViewportPolicy::with_max_width(width).overflow(overflow),
            )
            .expect("an exact fit should emit canonical output under every overflow policy");
            assert_eq!(output.text, canonical);
            assert_eq!(output.layout_profile, AsciiLayoutProfile::Canonical);
            assert_eq!(output.requested_layout_profile, AsciiLayoutProfile::Auto);
            assert!(!output.compact_attempted);
            assert!(!output.fallback.attempted);
            assert!(!output.overflowed);
        }
    }
}