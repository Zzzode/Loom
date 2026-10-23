fn default_report_preserves_text_projection_and_records_extent() {
    let report = render_report("flowchart LR\nA --> B", AsciiViewportPolicy::default())
        .expect("default report should render");
    assert_eq!(report.outcome, AsciiOutputOutcome::Primary);
    assert_eq!(report.projection, AsciiProjection::Diagrammatic);
    assert_eq!(report.primary_extent, report.emitted_extent);
    assert_eq!(report.primary_extent.width, 15);
    assert_eq!(report.primary_extent.height, 5);
    assert!(!report.overflowed);
    assert!(!report.fallback.attempted);
    assert_eq!(report.schema_version, ASCII_OUTPUT_SCHEMA_VERSION);
    assert_eq!(report.encoding, AsciiOutputEncoding::Plain);
    assert_eq!(report.as_text(), report.text);
}