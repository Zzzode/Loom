fn diagrammatic_fallback_preserves_typed_semantics() {
    let report = render_report(
        "flowchart LR\nA[Alpha] --> B[Beta]",
        AsciiViewportPolicy::with_max_width(5).overflow(OverflowPolicy::Fallback),
    )
    .expect("typed semantic fallback should be available");
    assert_eq!(report.outcome, AsciiOutputOutcome::Fallback);
    assert_eq!(report.projection, AsciiProjection::StructuredText);
    assert_eq!(report.lossiness.as_str(), "presentation_only");
    assert!(report.emitted_extent.width <= 5, "{}", report.text);
    let flattened = report.text.replace(['\n', ' ', '"'], "");
    assert!(flattened.contains("Alpha"), "{}", report.text);
    assert!(flattened.contains("Beta"), "{}", report.text);
}