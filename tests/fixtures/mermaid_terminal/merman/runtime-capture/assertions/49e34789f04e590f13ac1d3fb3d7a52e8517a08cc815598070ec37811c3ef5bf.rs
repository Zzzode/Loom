fn sequence_fallback_preserves_empty_authored_property_objects() {
    let source = concat!(
        "sequenceDiagram\n",
        "participant A\n",
        "properties A: {\"payload\": {}}\n",
        "participant B\n",
        "A->>B: A long authored message forcing a wide primary projection",
    );
    let report = render_report(
        source,
        AsciiViewportPolicy::with_max_width(12).overflow(OverflowPolicy::Fallback),
    )
    .expect("typed sequence fallback should preserve empty authored objects");

    assert_eq!(report.outcome, AsciiOutputOutcome::Fallback);
    assert_eq!(report.lossiness.as_str(), "presentation_only");
    assert!(report.emitted_extent.width <= 12, "{}", report.text);
    let flattened = report.text.replace('\n', "");
    assert!(
        flattened.contains("model.actors.A.properties.payload: {}"),
        "{}",
        report.text
    );
}