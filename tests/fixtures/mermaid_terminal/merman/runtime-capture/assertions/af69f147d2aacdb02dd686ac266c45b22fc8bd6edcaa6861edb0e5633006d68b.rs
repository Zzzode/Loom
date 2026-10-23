fn allow_returns_complete_wide_report() {
    let report = render_report(
        "flowchart LR\nA[Alpha] --> B[Beta]",
        AsciiViewportPolicy::with_max_width(5).overflow(OverflowPolicy::Allow),
    )
    .expect("Allow should return a complete wide result");
    assert_eq!(report.outcome, AsciiOutputOutcome::WideAllowed);
    assert!(report.overflowed);
    assert!(report.primary_extent.width > 5);
    assert!(report.text.contains("Alpha"));
    assert!(report.text.contains("Beta"));
}