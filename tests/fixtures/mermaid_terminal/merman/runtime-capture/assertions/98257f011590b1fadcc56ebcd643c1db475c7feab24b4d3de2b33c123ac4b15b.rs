fn fallback_uses_the_render_wide_work_ledger() {
    let source = "flowchart LR\nA[Alpha] --> B[Beta]";
    let resources = merman_ascii::AsciiResourcePolicy::default()
        .with_limit(
            merman_ascii::AsciiResourceLimitId::MaxLayoutWorkUnits,
            5_000,
        )
        .expect("limit should be valid");

    let error = render_report_with_resources(
        source,
        AsciiViewportPolicy::with_max_width(5).overflow(OverflowPolicy::Fallback),
        resources,
    )
    .expect_err("primary and fallback work must share one cumulative ledger");
    assert!(matches!(
        error,
        AsciiError::ResourceLimitExceeded(details)
            if details.limit == merman_ascii::AsciiResourceLimitId::MaxLayoutWorkUnits
                && details.actual > details.max
    ));
}