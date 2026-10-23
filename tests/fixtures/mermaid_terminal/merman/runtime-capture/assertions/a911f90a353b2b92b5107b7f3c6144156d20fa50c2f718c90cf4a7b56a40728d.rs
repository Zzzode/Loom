fn error_policy_returns_independent_width_error() {
    let error = render_report(
        "flowchart LR\nA[Alpha] --> B[Beta]",
        AsciiViewportPolicy::with_max_width(5).overflow(AsciiOverflowPolicy::Error),
    )
    .expect_err("Error must reject an over-wide primary projection");
    assert!(matches!(
        error,
        AsciiError::WidthOverflow {
            max_width: 5,
            actual_width: _,
            ..
        }
    ));
}