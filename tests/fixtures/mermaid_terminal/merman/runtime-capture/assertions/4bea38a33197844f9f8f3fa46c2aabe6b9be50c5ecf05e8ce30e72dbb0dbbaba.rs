fn diagrammatic_fallback_keeps_terminal_controls_visible() {
    let report = render_report(
        "flowchart LR\nA[\"alpha\u{202e}beta\"] --> B",
        AsciiViewportPolicy::with_max_width(12).overflow(OverflowPolicy::Fallback),
    )
    .expect("typed flowchart fallback should normalize terminal controls");

    assert_eq!(report.outcome, AsciiOutputOutcome::Fallback);
    assert!(report.emitted_extent.width <= 12, "{}", report.text);
    assert!(!report.text.contains('\u{202e}'), "{}", report.text);
    assert!(report.text.contains("\\u{202E}"), "{}", report.text);
}