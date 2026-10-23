fn auto_applies_error_or_structured_fallback_to_the_selected_primary_extent() {
    let model = parse_model(LONG_NODE_CHAIN);
    let options = AsciiRenderOptions::ascii().with_layout_profile(AsciiLayoutProfile::Auto);
    let selected = render_model_report(&model, &options, AsciiViewportPolicy::with_max_width(60))
        .expect("Allow should return the complete compact diagram");
    let error = render_model_report(
        &model,
        &options,
        AsciiViewportPolicy::with_max_width(60).overflow(OverflowPolicy::Error),
    )
    .expect_err("neither primary candidate fits sixty cells");
    assert!(matches!(
        error,
        AsciiError::WidthOverflow { max_width: 60, actual_width, .. }
            if actual_width == selected.primary_extent.width
    ));

    let fallback = render_model_report(
        &model,
        &options,
        AsciiViewportPolicy::with_max_width(60).overflow(OverflowPolicy::Fallback),
    )
    .expect("the complete structured graph should fit sixty cells");
    assert_eq!(fallback.primary_extent, selected.primary_extent);
    assert_eq!(fallback.layout_profile, AsciiLayoutProfile::Compact);
    assert_eq!(fallback.requested_layout_profile, AsciiLayoutProfile::Auto);
    assert!(fallback.compact_attempted);
    assert!(fallback.fallback.attempted);
    assert!(fallback.overflowed);
    assert_eq!(fallback.outcome, AsciiOutputOutcome::Fallback);
    assert_eq!(fallback.projection, AsciiProjection::StructuredText);
    assert!(fallback.emitted_extent.width <= 60);
    for word in ["many", "words", "sometime"] {
        assert_eq!(fallback.text.matches(word).count(), 4, "{}", fallback.text);
    }
}