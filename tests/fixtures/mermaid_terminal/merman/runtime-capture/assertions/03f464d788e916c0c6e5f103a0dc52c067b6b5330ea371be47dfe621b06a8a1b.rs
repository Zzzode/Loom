fn explicit_canonical_remains_fixed_even_when_compact_would_fit() {
    let model = parse_model(LONG_NODE_CHAIN);
    let options = AsciiRenderOptions::ascii();
    let canonical = render_model(&model, &options).expect("canonical graph should render");
    let output = render_model_report(&model, &options, AsciiViewportPolicy::with_max_width(120))
        .expect("fixed Canonical with Allow should keep its full width");
    assert_eq!(output.text, canonical);
    assert_eq!(output.layout_profile, AsciiLayoutProfile::Canonical);
    assert_eq!(
        output.requested_layout_profile,
        AsciiLayoutProfile::Canonical
    );
    assert!(!output.compact_attempted);
    assert_eq!(output.outcome, AsciiOutputOutcome::WideAllowed);
    assert!(matches!(
        render_model_report(
            &model,
            &options,
            AsciiViewportPolicy::with_max_width(120).overflow(OverflowPolicy::Error),
        ),
        Err(AsciiError::WidthOverflow { .. })
    ));
}