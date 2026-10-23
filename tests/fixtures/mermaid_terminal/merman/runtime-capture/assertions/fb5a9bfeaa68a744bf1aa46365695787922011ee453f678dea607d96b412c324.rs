fn diagrammatic_fallback_keeps_typed_flowchart_edge_markers() {
    let report = render_report(
        "graph LR;A o--x B;",
        AsciiViewportPolicy::with_max_width(12).overflow(OverflowPolicy::Fallback),
    )
    .expect("typed flowchart fallback should preserve edge semantics");
    assert_eq!(report.outcome, AsciiOutputOutcome::Fallback);
    let flattened = report.text.replace('\n', "");
    assert!(flattened.contains("startMarker"), "{}", report.text);
    assert!(flattened.contains("circle"), "{}", report.text);
    assert!(flattened.contains("endMarker"), "{}", report.text);
    assert!(flattened.contains("cross"), "{}", report.text);
}