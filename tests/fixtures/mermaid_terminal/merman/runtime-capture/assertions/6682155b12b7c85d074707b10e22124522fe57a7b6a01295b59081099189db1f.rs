fn structured_fallback_reflows_complete_text() {
    let report = render_report(
        "timeline\ntitle Basic\n2024 : Event with a long authored value",
        AsciiViewportPolicy::with_max_width(10).overflow(OverflowPolicy::Fallback),
    )
    .expect("structured text should have a bounded fallback");
    assert_eq!(report.outcome, AsciiOutputOutcome::Fallback);
    assert_eq!(report.projection, AsciiProjection::StructuredText);
    assert!(report.fallback.attempted);
    assert_eq!(
        report.fallback.reason.map(|reason| reason.as_str()),
        Some("primary_overflow")
    );
    assert!(report.emitted_extent.width <= 10);
    assert!(
        report.text.replace('\n', "").contains("Basic"),
        "{}",
        report.text
    );
    assert!(
        report.text.replace('\n', "").contains("Event"),
        "{}",
        report.text
    );
}